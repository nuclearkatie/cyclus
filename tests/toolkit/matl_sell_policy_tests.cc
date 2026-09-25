
#include <gtest/gtest.h>

#include "toolkit/matl_sell_policy.h"

#include "composition.h"
#include "material.h"
#include "bid.h"
#include "request.h"
#include "error.h"
#include "pyne.h"
#include "package.h"
#include "platform.h"
#if CYCLUS_HAS_COIN
#include "prog_solver.h"
#endif

#include "test_context.h"
#include "test_agents/test_facility.h"

namespace cyclus {
namespace toolkit {

class TestComp : public Composition {
 public:
  TestComp() {}
};

class MatlSellPolicyTests: public ::testing::Test {
 protected:
  TestContext tc;
  TestFacility* fac1;
  double cap, qty;
  ResBuf<Material> buff;
  Composition::Ptr comp, comp1;
  Material::Ptr mat, mat1;

  virtual void SetUp() {
    fac1 = new TestFacility(tc.get());
    cap = 5;
    qty = 3;
    buff.capacity(cap);
    CompMap v;
    v[pyne::nucname::id("H1")] = 1;
    comp = Composition::CreateFromAtom(v);
    mat = Material::CreateUntracked(qty, comp);
    buff.Push(mat);
    comp1 = Composition::CreateFromAtom(v);
    mat1 = Material::CreateUntracked(qty, comp1);
  }

  virtual void TearDown() {
    delete fac1;
  }
};


TEST_F(MatlSellPolicyTests, Init) {
  MatlSellPolicy p;

  // defaults
  p.Init(fac1, &buff, "");
  ASSERT_FALSE(p.Excl());
  ASSERT_FLOAT_EQ(p.Limit(), qty);

  // limit & excl
  p.Init(fac1, &buff, "", 1.5, false, 1);
  ASSERT_TRUE(p.Excl());
  ASSERT_FLOAT_EQ(p.Limit(), 1.5);

  // throughput = 2.5, quantize = 2, limit = 2
  p.Init(fac1, &buff, "", qty - 0.5, false, qty - 1);
  ASSERT_FLOAT_EQ(p.Limit(), qty - 1);
}

TEST_F(MatlSellPolicyTests, StartStop) {
  MatlSellPolicy p;
  p.Init(NULL, &buff, "");
  ASSERT_THROW(p.Start(), ValueError);
  ASSERT_THROW(p.Stop(), ValueError);
}

TEST_F(MatlSellPolicyTests, Bids) {
  MatlSellPolicy p;
  std::string commod("commod");
  CommodMap<Material>::type reqs;
  reqs[commod] = std::vector<Request<Material>*>();
  Request<Material>* req = Request<Material>::Create(mat1, fac1, commod);
  reqs[commod].push_back(req);
  std::set<BidPortfolio<Material>::Ptr> obs;

  // basic
  p.Init(NULL, &buff, "").Set(commod);
  obs = p.GetMatlBids(reqs);
  ASSERT_EQ(obs.size(), 1);
  ASSERT_EQ((*obs.begin())->bids().size(), 1);
  ASSERT_FLOAT_EQ((*(*obs.begin())->bids().begin())->offer()->quantity(),
                  mat->quantity());
  ASSERT_EQ((*(*obs.begin())->bids().begin())->offer()->comp(), comp);

  // excl and ignore_comp
    // Qty = 3, Throughput = 3, Quanta = 1.5 -> 2 transactions of 1 quanta
  p.Init(NULL, &buff, "", qty, true, qty / 2).Set(commod);
  obs = p.GetMatlBids(reqs);
  ASSERT_EQ(obs.size(), 1);
  ASSERT_EQ((*obs.begin())->bids().size(), 2);
  ASSERT_FLOAT_EQ((*(*obs.begin())->bids().begin())->offer()->quantity(),
                  qty / 2);
  ASSERT_EQ((*(*obs.begin())->bids().begin())->offer()->comp(), comp1);

  // If available quantity is bigger than the quanta only an integer number of quanta is offered
  // Qty = 3, Throughput = 3, Quanta = 2 -> only 1 transactions of 1 quanta
  p.Init(NULL, &buff, "", qty, true, 2).Set(commod);
  obs = p.GetMatlBids(reqs);
  ASSERT_EQ(obs.size(), 1);
  ASSERT_EQ((*obs.begin())->bids().size(), 1);
  ASSERT_FLOAT_EQ((*(*obs.begin())->bids().begin())->offer()->quantity(),
                  2);
  ASSERT_EQ((*(*obs.begin())->bids().begin())->offer()->comp(), comp1);


  // quantize bigger than the quantity in storage
   // Qty = 3, Throughput = 3, Quanta = 6 -> No transaction
  p.Init(NULL, &buff, "", qty, true, qty * 2).Set(commod);
  obs = p.GetMatlBids(reqs);
  ASSERT_EQ(obs.size(), 0);
  delete req;
}

TEST_F(MatlSellPolicyTests, Trades) {
  MatlSellPolicy p;
  std::string commod("commod");
  std::vector<Trade<Material> > trades;
  std::vector<std::pair<Trade<Material>, Material::Ptr> > obs;

  Request<Material>* req = Request<Material>::Create(mat1, fac1, commod);
  Bid<Material>* bid = Bid<Material>::Create(req, mat, fac1);
  Trade<Material> trade(req, bid, 1);
  trades.push_back(trade);

  // basic
  p.Init(NULL, &buff, "").Set(commod);
  p.GetMatlTrades(trades, obs);
  ASSERT_EQ(obs.size(), 1);
  ASSERT_EQ(obs.begin()->second->comp(), comp);

  // ignore comp
  obs.clear();
  p.Init(NULL, &buff, "", qty, true).Set(commod);
  p.GetMatlTrades(trades, obs);
  ASSERT_EQ(obs.size(), 1);
  ASSERT_EQ(obs.begin()->second->comp(), comp1);

  delete bid;
  delete req;
}

TEST_F(MatlSellPolicyTests, Package) {
  using cyclus::QueryResult;

  int dur = 5;
  double throughput = 1;

  cyclus::MockSim sim(dur);
  cyclus::Agent* a = new TestFacility(sim.context());

  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").capacity(2).Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  cyclus::toolkit::ResBuf<cyclus::Material> buf;

  double qty = 5;
  CompMap cm;
  cm[922350000] = 0.05;
  cm[922380000] = 0.95;
  Composition::Ptr comp = Composition::CreateFromMass(cm);
  mat = Material::Create(a, qty, comp, Package::unpackaged_name());

  buf.Push(mat);

  sim.context()->AddPackage("foo", 1, 2, "first");
  Package::Ptr p = sim.context()->GetPackage("foo");

  cyclus::toolkit::MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", 4, false, 0, p->name())
          .Set("commod").Start();

  EXPECT_NO_THROW(sim.Run());

  QueryResult qr_trans = sim.db().Query("Transactions", NULL);
  EXPECT_EQ(3, qr_trans.rows.size());

  EXPECT_EQ(0, qr_trans.GetVal<int>("Time", 0));
  EXPECT_EQ(1, qr_trans.GetVal<int>("Time", 1));
  EXPECT_EQ(2, qr_trans.GetVal<int>("Time", 2));

  // Resource 0 is the material of 5 that we first created.
  QueryResult qr_all_res = sim.db().Query("Resources", NULL);
  EXPECT_EQ(5, qr_all_res.GetVal<double>("Quantity", 0));

  // Then check that three packaged materials were created of size 2, 2, and 1
  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("PackageName", "==", std::string("foo")));
  QueryResult qr_pkgd_res = sim.db().Query("Resources", &conds);
  EXPECT_EQ(qr_pkgd_res.rows.size(), 3);

  std::vector<double> pkged_res = {qr_pkgd_res.GetVal<double>("Quantity", 0),
                                   qr_pkgd_res.GetVal<double>("Quantity", 1),
                                   qr_pkgd_res.GetVal<double>("Quantity", 2)};
  std::sort(pkged_res.begin(), pkged_res.end(), std::greater<double>());

  std::vector<double> exp = {2, 2, 1};

  EXPECT_EQ(exp, pkged_res);

  // All material should have been transacted, including the resource of size 1
  EXPECT_EQ(0, buf.quantity());
}

TEST_F(MatlSellPolicyTests, TransportUnit) {
  using cyclus::QueryResult;

  int dur = 5;
  double throughput = 1;

  cyclus::MockSim sim(dur);
  cyclus::Agent* a = new TestFacility(sim.context());

  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  cyclus::toolkit::ResBuf<cyclus::Material> buf;

  double qty = 5;
  CompMap cm;
  cm[922350000] = 0.05;
  cm[922380000] = 0.95;
  Composition::Ptr comp = Composition::CreateFromMass(cm);
  mat = Material::Create(a, qty, comp, Package::unpackaged_name());

  buf.Push(mat);

  sim.context()->AddPackage("foo", 1, 2, "first");
  Package::Ptr p = sim.context()->GetPackage("foo");
  sim.context()->AddTransportUnit("foo-truck", 3, 3, "first");
  TransportUnit::Ptr tu = sim.context()->GetTransportUnit("foo-truck");

  cyclus::toolkit::MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", 10, false, 0, p->name(), tu->name())
          .Set("commod").Start();

  EXPECT_NO_THROW(sim.Run());

  QueryResult qr_trans = sim.db().Query("Transactions", NULL);
  EXPECT_EQ(3, qr_trans.rows.size());

  EXPECT_EQ(0, qr_trans.GetVal<int>("Time", 0));
  EXPECT_EQ(0, qr_trans.GetVal<int>("Time", 1));
  EXPECT_EQ(0, qr_trans.GetVal<int>("Time", 2));

  // the order of trades that happen in the same time step is not guaranteed,
  // so we can't use database order to confirm which resource (sizes 2, 2, 1)
  // will appear in which order. Thus make a set, sort high-low, then compare
  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("PackageName", "==", std::string("foo")));
  QueryResult qr_res = sim.db().Query("Resources", &conds);
  std::vector<double> pkgd_trades = {qr_res.GetVal<double>("Quantity", 0),
                                     qr_res.GetVal<double>("Quantity", 1),
                                     qr_res.GetVal<double>("Quantity", 2)};

  std::sort(pkgd_trades.begin(), pkgd_trades.end(), std::greater<double>());
  std::vector<double> exp = {2, 2, 1};
  EXPECT_EQ(exp, pkgd_trades);

  // All material should have been transacted, including the resource of size 1
  EXPECT_EQ(0, buf.quantity());
}

TEST_F(MatlSellPolicyTests, PackageLimit) {
  using cyclus::QueryResult;

  int dur = 2;

  cyclus::MockSim sim(dur);
  cyclus::Agent* a = new TestFacility(sim.context());

  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  cyclus::toolkit::ResBuf<cyclus::Material> buf;

  double qty = cyclus::CY_LARGE_DOUBLE;
  CompMap cm;
  cm[922380000] = 1;
  Composition::Ptr comp = Composition::CreateFromMass(cm);
  mat = Material::Create(a, qty, comp, Package::unpackaged_name());

  buf.Push(mat);

  sim.context()->AddPackage("foo", 1, 2, "first");
  Package::Ptr p = sim.context()->GetPackage("foo");

  cyclus::toolkit::MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", cyclus::CY_LARGE_DOUBLE, false, 0, p->name())
          .Set("commod").Start();

  ASSERT_THROW(sim.Run(), cyclus::ValueError);
}

TEST_F(MatlSellPolicyTests, PackageFailedTrade) {
  using cyclus::QueryResult;

  int dur = 1;

  cyclus::MockSim sim(dur);
  cyclus::Agent* a = new TestFacility(sim.context());

  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  // Add two sinks that both want commod
  sim.AddSink("commod").capacity(5).Finalize();
  sim.AddSink("commod").capacity(5).Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  cyclus::toolkit::ResBuf<cyclus::Material> buf;

  double qty = 7;
  CompMap cm;
  cm[922380000] = 1;
  Composition::Ptr comp = Composition::CreateFromMass(cm);
  mat = Material::Create(a, qty, comp, Package::unpackaged_name());

  buf.Push(mat);

  sim.context()->AddPackage("foo", 5, 5, "first");
  Package::Ptr p = sim.context()->GetPackage("foo");

  cyclus::toolkit::MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", cyclus::CY_LARGE_DOUBLE, false, 0, p->name())
          .Set("commod").Start();

  EXPECT_NO_THROW(sim.Run());

  // One of the sinks is given 5 (their request), and the other is given 2.
  // Two kg can't be packaged, so the trade fails and the material traded
  // is an untracked zero quantity. Therefore, only one transaction shows up
  QueryResult qr_trans = sim.db().Query("Transactions", NULL);
  EXPECT_EQ(1, qr_trans.rows.size());
  EXPECT_EQ(0, qr_trans.GetVal<int>("Time", 0));

  std::vector<cyclus::Cond> conds;
  conds.push_back(cyclus::Cond("PackageName", "==", std::string("foo")));
  QueryResult qr_res = sim.db().Query("Resources", &conds);
  EXPECT_EQ(1, qr_res.rows.size());
  EXPECT_EQ(5, qr_res.GetVal<double>("Quantity", 0));

  // Not all material should have been transacted, one trade of 5 is the only
  // non-zero trade
  EXPECT_EQ(2, buf.quantity());

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Discrete trading: resource objects are bid and traded whole
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Fills buf with untracked objects of the given sizes, oldest first.
static std::vector<Material::Ptr> FillBuf(ResBuf<Material>& buf,
                                          std::vector<double> sizes,
                                          Composition::Ptr comp) {
  std::vector<Material::Ptr> objs;
  for (int i = 0; i < sizes.size(); ++i) {
    objs.push_back(Material::CreateUntracked(sizes[i], comp));
    buf.Push(objs.back());
  }
  return objs;
}

// Collects every bid offer across a set of portfolios, keyed by quantity.
static std::multiset<double> BidQtys(
    const std::set<BidPortfolio<Material>::Ptr>& ports) {
  std::multiset<double> qtys;
  std::set<BidPortfolio<Material>::Ptr>::const_iterator pit;
  for (pit = ports.begin(); pit != ports.end(); ++pit) {
    std::set<Bid<Material>*>::const_iterator bit;
    for (bit = (*pit)->bids().begin(); bit != (*pit)->bids().end(); ++bit) {
      qtys.insert((*bit)->offer()->quantity());
    }
  }
  return qtys;
}

TEST_F(MatlSellPolicyTests, DiscreteInit) {
  ResBuf<Material> buf;
  FillBuf(buf, {9.7, 9.5, 4.0}, comp);

  MatlSellPolicy p;
  p.Init(fac1, &buf, "", CY_LARGE_DOUBLE, false, 0,
         Package::unpackaged_name(), TransportUnit::unrestricted_name(),
         true);
  EXPECT_TRUE(p.Discrete());
  EXPECT_TRUE(p.Excl());
  EXPECT_DOUBLE_EQ(p.Limit(), 9.7 + 9.5 + 4.0);

  // throughput 14: oldest first with skipping -> 9.7, skip 9.5, then 4.0
  p.Init(fac1, &buf, "", 14, false, 0, Package::unpackaged_name(),
         TransportUnit::unrestricted_name(), true);
  EXPECT_DOUBLE_EQ(p.Limit(), 9.7 + 4.0);

  // discrete and quantize are mutually exclusive
  MatlSellPolicy q;
  EXPECT_THROW(q.Init(fac1, &buf, "", CY_LARGE_DOUBLE, false, 1,
                      Package::unpackaged_name(),
                      TransportUnit::unrestricted_name(), true),
               ValueError);

  // bulk buffers merge objects, so they can't be traded discretely
  ResBuf<Material> bulk(true);
  MatlSellPolicy r;
  EXPECT_THROW(r.Init(fac1, &bulk, "", CY_LARGE_DOUBLE, false, 0,
                      Package::unpackaged_name(),
                      TransportUnit::unrestricted_name(), true),
               ValueError);
}

TEST_F(MatlSellPolicyTests, DiscreteBids) {
  ResBuf<Material> buf;
  FillBuf(buf, {9.7, 9.5, 4.0}, comp);

  std::string commod("commod");
  CommodMap<Material>::type reqs;
  Request<Material>* req =
      Request<Material>::Create(Material::CreateUntracked(15, comp1), fac1,
                                commod);
  reqs[commod].push_back(req);

  MatlSellPolicy p;
  p.Init(NULL, &buf, "", CY_LARGE_DOUBLE, false, 0, Package::unpackaged_name(),
         TransportUnit::unrestricted_name(), true)
      .Set(commod);
  std::set<BidPortfolio<Material>::Ptr> ports = p.GetMatlBids(reqs);

  // 15 kg request: 9.7 fits, 9.5 doesn't fit the remaining 5.3 so it is
  // skipped, 4.0 fits. One portfolio per object, capped at its quantity.
  std::multiset<double> exp = {4.0, 9.7};
  EXPECT_EQ(BidQtys(ports), exp);
  ASSERT_EQ(ports.size(), 2);
  std::set<BidPortfolio<Material>::Ptr>::iterator pit;
  for (pit = ports.begin(); pit != ports.end(); ++pit) {
    ASSERT_EQ((*pit)->bids().size(), 1);
    Bid<Material>* b = *(*pit)->bids().begin();
    EXPECT_TRUE(b->exclusive());
    ASSERT_EQ((*pit)->constraints().size(), 1);
    EXPECT_DOUBLE_EQ((*pit)->constraints().begin()->capacity(),
                     b->offer()->quantity());
  }

  // bidding does not remove anything from the buffer
  EXPECT_EQ(buf.count(), 3);

  // a request smaller than every object gets no bids
  CommodMap<Material>::type small_reqs;
  Request<Material>* small =
      Request<Material>::Create(Material::CreateUntracked(1, comp1), fac1,
                                commod);
  small_reqs[commod].push_back(small);
  EXPECT_TRUE(p.GetMatlBids(small_reqs).empty());

  delete req;
  delete small;
}

TEST_F(MatlSellPolicyTests, DiscreteTrades) {
  ResBuf<Material> buf;
  std::vector<Material::Ptr> objs = FillBuf(buf, {9.7, 9.5, 4.0}, comp);

  std::string commod("commod");
  CommodMap<Material>::type reqs;
  Request<Material>* req =
      Request<Material>::Create(Material::CreateUntracked(15, comp1), fac1,
                                commod);
  reqs[commod].push_back(req);

  MatlSellPolicy p;
  p.Init(NULL, &buf, "", CY_LARGE_DOUBLE, false, 0, Package::unpackaged_name(),
         TransportUnit::unrestricted_name(), true)
      .Set(commod);
  std::set<BidPortfolio<Material>::Ptr> ports = p.GetMatlBids(reqs);

  // accept every bid in full
  std::vector<Trade<Material>> trades;
  std::set<BidPortfolio<Material>::Ptr>::iterator pit;
  for (pit = ports.begin(); pit != ports.end(); ++pit) {
    Bid<Material>* b = *(*pit)->bids().begin();
    trades.push_back(Trade<Material>(req, b, b->offer()->quantity()));
  }
  std::vector<std::pair<Trade<Material>, Material::Ptr>> obs;
  p.GetMatlTrades(trades, obs);

  // the responses are the buffer's original objects, not copies or pieces
  ASSERT_EQ(obs.size(), 2);
  std::set<Material::Ptr> sent;
  for (int i = 0; i < obs.size(); ++i) sent.insert(obs[i].second);
  EXPECT_EQ(sent.count(objs[0]), 1);  // 9.7
  EXPECT_EQ(sent.count(objs[2]), 1);  // 4.0

  // the skipped 9.5 kg object is untouched
  ASSERT_EQ(buf.count(), 1);
  EXPECT_EQ(buf.Peek(), objs[1]);
  EXPECT_DOUBLE_EQ(buf.quantity(), 9.5);

  delete req;
}

TEST_F(MatlSellPolicyTests, DiscreteTradeErrors) {
  ResBuf<Material> buf;
  FillBuf(buf, {5.0}, comp);

  std::string commod("commod");
  CommodMap<Material>::type reqs;
  Request<Material>* req =
      Request<Material>::Create(Material::CreateUntracked(10, comp1), fac1,
                                commod);
  reqs[commod].push_back(req);

  MatlSellPolicy p;
  p.Init(NULL, &buf, "", CY_LARGE_DOUBLE, false, 0, Package::unpackaged_name(),
         TransportUnit::unrestricted_name(), true)
      .Set(commod);
  std::set<BidPortfolio<Material>::Ptr> ports = p.GetMatlBids(reqs);
  Bid<Material>* b = *(*ports.begin())->bids().begin();
  std::vector<std::pair<Trade<Material>, Material::Ptr>> obs;

  // a partial match of an exclusive whole-object bid is an error
  std::vector<Trade<Material>> partial = {Trade<Material>(req, b, 2.0)};
  EXPECT_THROW(p.GetMatlTrades(partial, obs), ValueError);
  EXPECT_EQ(buf.count(), 1);  // nothing was removed

  // a trade on a bid this policy never made is an error
  Bid<Material>* foreign = Bid<Material>::Create(req, mat, fac1);
  std::vector<Trade<Material>> bad = {Trade<Material>(req, foreign, 3.0)};
  EXPECT_THROW(p.GetMatlTrades(bad, obs), StateError);

  delete foreign;
  delete req;
}

// Runs one time step of a discrete sell policy against sinks and returns the
// traded resources as (resource id, quantity, package name).
struct Traded {
  int res_id;
  double qty;
  std::string pkg;
  bool operator<(const Traded& o) const { return res_id < o.res_id; }
};

static std::vector<Traded> TradedResources(cyclus::MockSim& sim) {
  std::vector<Traded> out;
  QueryResult trans = sim.db().Query("Transactions", NULL);
  for (int i = 0; i < trans.rows.size(); ++i) {
    int res_id = trans.GetVal<int>("ResourceId", i);
    std::vector<cyclus::Cond> conds;
    conds.push_back(cyclus::Cond("ResourceId", "==", res_id));
    QueryResult res = sim.db().Query("Resources", &conds);
    out.push_back({res_id, res.GetVal<double>("Quantity", 0),
                   res.GetVal<std::string>("PackageName", 0)});
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Two sinks that could each take everything. Each object must go to exactly
// one sink, whole, with its original resource id.
static void DiscreteTwoSinks(ExchangeSolver* solver) {
  cyclus::MockSim sim(1);
  if (solver != NULL) sim.context()->solver(solver);
  cyclus::Agent* a = new TestFacility(sim.context());
  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").capacity(20).Finalize();
  sim.AddSink("commod").capacity(20).Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  CompMap cm;
  cm[922380000] = 1;
  Composition::Ptr c = Composition::CreateFromMass(cm);
  Material::Ptr m1 = Material::Create(a, 5, c, Package::unpackaged_name());
  Material::Ptr m2 = Material::Create(a, 3, c, Package::unpackaged_name());
  ResBuf<Material> buf;
  buf.Push(m1);
  buf.Push(m2);
  int id1 = m1->state_id();
  int id2 = m2->state_id();

  MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", CY_LARGE_DOUBLE, false, 0,
               Package::unpackaged_name(), TransportUnit::unrestricted_name(),
               true)
      .Set("commod")
      .Start();
  EXPECT_NO_THROW(sim.Run());

  std::vector<Traded> traded = TradedResources(sim);
  ASSERT_EQ(traded.size(), 2);  // not 4: no object went to both sinks
  std::set<int> ids = {traded[0].res_id, traded[1].res_id};
  std::set<int> exp_ids = {id1, id2};
  EXPECT_EQ(ids, exp_ids);  // the original objects, unmodified
  EXPECT_EQ(buf.count(), 0);
}

TEST_F(MatlSellPolicyTests, DiscreteTwoSinksGreedy) {
  DiscreteTwoSinks(NULL);  // MockSim default: greedy solver
}

#if CYCLUS_HAS_COIN
TEST_F(MatlSellPolicyTests, DiscreteTwoSinksMILP) {
  DiscreteTwoSinks(new ProgSolver("cbc"));  // context deletes the solver
}
#endif

// One 8 kg sink and objects of 5, 4, 3 (oldest first): 5 is bid, 4 no longer
// fits the remaining 3 and is skipped, 3 is bid.
static void DiscreteSkip(ExchangeSolver* solver) {
  cyclus::MockSim sim(1);
  if (solver != NULL) sim.context()->solver(solver);
  cyclus::Agent* a = new TestFacility(sim.context());
  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").capacity(8).Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  CompMap cm;
  cm[922380000] = 1;
  Composition::Ptr c = Composition::CreateFromMass(cm);
  ResBuf<Material> buf;
  Material::Ptr m5 = Material::Create(a, 5, c, Package::unpackaged_name());
  Material::Ptr m4 = Material::Create(a, 4, c, Package::unpackaged_name());
  Material::Ptr m3 = Material::Create(a, 3, c, Package::unpackaged_name());
  buf.Push(m5);
  buf.Push(m4);
  buf.Push(m3);

  MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", CY_LARGE_DOUBLE, false, 0,
               Package::unpackaged_name(), TransportUnit::unrestricted_name(),
               true)
      .Set("commod")
      .Start();
  EXPECT_NO_THROW(sim.Run());

  std::vector<Traded> traded = TradedResources(sim);
  ASSERT_EQ(traded.size(), 2);
  std::multiset<double> qtys = {traded[0].qty, traded[1].qty};
  std::multiset<double> exp = {3, 5};
  EXPECT_EQ(qtys, exp);
  ASSERT_EQ(buf.count(), 1);
  EXPECT_EQ(buf.Peek(), m4);
}

TEST_F(MatlSellPolicyTests, DiscreteSkipGreedy) {
  DiscreteSkip(NULL);
}

#if CYCLUS_HAS_COIN
TEST_F(MatlSellPolicyTests, DiscreteSkipMILP) {
  DiscreteSkip(new ProgSolver("cbc"));
}
#endif

// A buffer that keeps packaging: packaged objects are traded in their own
// package, as the same resource, even though the policy has no sell package.
TEST_F(MatlSellPolicyTests, DiscreteKeepPackaging) {
  cyclus::MockSim sim(1);
  cyclus::Agent* a = new TestFacility(sim.context());
  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").capacity(20).Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  sim.context()->AddPackage("foo", 4, 6, "normal");
  CompMap cm;
  cm[922380000] = 1;
  Composition::Ptr c = Composition::CreateFromMass(cm);
  Material::Ptr m1 = Material::Create(a, 5.5, c, "foo");
  Material::Ptr m2 = Material::Create(a, 4.5, c, "foo");

  ResBuf<Material> buf(false, true);  // keep packaging
  buf.Push(m1);
  buf.Push(m2);
  std::set<int> exp_ids = {m1->state_id(), m2->state_id()};

  MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", CY_LARGE_DOUBLE, false, 0,
               Package::unpackaged_name(), TransportUnit::unrestricted_name(),
               true)
      .Set("commod")
      .Start();
  EXPECT_NO_THROW(sim.Run());

  std::vector<Traded> traded = TradedResources(sim);
  ASSERT_EQ(traded.size(), 2);
  std::set<int> ids;
  for (int i = 0; i < traded.size(); ++i) {
    EXPECT_EQ(traded[i].pkg, "foo");
    ids.insert(traded[i].res_id);
  }
  EXPECT_EQ(ids, exp_ids);  // no repackaging: same resources
}

// With a sell package, only objects that fit whole in one package are sold,
// and each is wrapped in the package without being split.
TEST_F(MatlSellPolicyTests, DiscreteSellPackage) {
  cyclus::MockSim sim(1);
  cyclus::Agent* a = new TestFacility(sim.context());
  sim.context()->AddPrototype(a->prototype(), a);
  sim.agent = sim.context()->CreateAgent<cyclus::Agent>(a->prototype());
  sim.AddSink("commod").capacity(20).Finalize();
  TestFacility* fac = dynamic_cast<TestFacility*>(sim.agent);

  sim.context()->AddPackage("foo", 4, 6, "first");
  CompMap cm;
  cm[922380000] = 1;
  Composition::Ptr c = Composition::CreateFromMass(cm);
  Material::Ptr m7 = Material::Create(a, 7, c, Package::unpackaged_name());
  Material::Ptr m5 = Material::Create(a, 5, c, Package::unpackaged_name());
  ResBuf<Material> buf;
  buf.Push(m7);
  buf.Push(m5);

  MatlSellPolicy sellpol;
  sellpol.Init(fac, &buf, "buf", CY_LARGE_DOUBLE, false, 0, "foo",
               TransportUnit::unrestricted_name(), true)
      .Set("commod")
      .Start();
  EXPECT_NO_THROW(sim.Run());

  std::vector<Traded> traded = TradedResources(sim);
  ASSERT_EQ(traded.size(), 1);
  EXPECT_DOUBLE_EQ(traded[0].qty, 5);
  EXPECT_EQ(traded[0].pkg, "foo");
  ASSERT_EQ(buf.count(), 1);
  EXPECT_EQ(buf.Peek(), m7);  // too big for one package, left untouched
}

}
}
