#ifndef CYCLUS_SRC_TOOLKIT_MATL_SELL_POLICY_H_
#define CYCLUS_SRC_TOOLKIT_MATL_SELL_POLICY_H_

#include <map>
#include <string>
#include <vector>

#include "composition.h"
#include "material.h"
#include "res_buf.h"
#include "trader.h"
#include "package.h"

namespace cyclus {
namespace toolkit {

/// MatlSellPolicy performs semi-automatic inventory management of a material
/// buffer by making offers and trading away materials in an attempt to empty
/// the buffer's inventory every time step.
///
/// For simple behavior, policies virtually eliminate the need to write any code
/// for resource exchange. Just assign a few policies to work with a few buffers
/// and focus on writing the physics and other behvavior of your agent.  Typical
/// usage goes something like this:
///
/// @code
/// class YourAgent : public Facility {
///  public:
///   ...
///
///   void EnterNotify() {
///     Facility::EnterNotify(); // always do this first
///
///     policy_.Init(this, &outbuf_, "outbuf-label").Set(outcommod).Start();
///   }
///   ...
///
///  private:
///   MatlSellPolicy policy_;
///   ResBuf<Material> outbuf_;
///    ...
/// }
/// @endcode
///
/// The policy needs to be initialized with its owning agent and the material
/// buffer that is is managing. It also needs to be activated by calling the
/// Start function for it to begin participation in resource exchange.  And
/// don't forget to add some commodities to offer on by calling Set.  All policy
/// configuration should usually occur in the agent's EnterNotify member
/// function.
///
/// @warn When a policy's managing agent is deallocated, you MUST either
/// call the policy's Stop function or delete the policy. Otherwise SEGFAULT.
class MatlSellPolicy : public Trader {
 public:
  /// Creates an uninitialized policy.  The Init function MUST be called before
  /// anything else is done with the policy.
  MatlSellPolicy();

  virtual ~MatlSellPolicy();

  /// Configures the policy to keep buf empty every time step by offering away
  /// as much material as possible.  If quantize is greater than zero, the
  /// policy will make exclusive, integral quantize kg offers.  Otherwise, a
  /// single offer will be sent each time step to empty the buffer's entire
  /// inventory.
  /// @param manager the agent
  /// @param buf the resource buffer
  /// @param name a unique name identifying this policy
  /// @param throughput a maximum throughput per time step for material leaving
  /// buf
  /// @ignore_comp ignore the composition of material in buf. When making bids,
  /// requested material compositions will be used. When making trades,
  /// material in the buffer will have its composition transmuted to match the
  /// requested material.
  /// @param quantize If quantize is greater than zero, the policy will make
  /// exclusive, integral quantize kg bids. Otherwise, single bids will
  /// be sent matching the requested quantity.
  /// @param package_name the package that traded material is packaged into
  /// @param transport_unit_name the transport unit that restricts how many
  /// packages can be shipped
  /// @param discrete If true, resource objects in buf are traded whole: each
  /// object is offered as its own exclusive bid of exactly its quantity, and
  /// the traded object is the buffer's original object (never split or
  /// combined). Objects are offered oldest first; an object that does not
  /// fit in the remaining request (or throughput) is skipped and newer
  /// objects are considered. If buf keeps packaging, packaged objects are
  /// traded in their existing package. Cannot be combined with quantize or
  /// used with a bulk buffer.
  /// @{
  MatlSellPolicy& Init(Agent* manager, ResBuf<Material>* buf, std::string name);
  MatlSellPolicy& Init(Agent* manager, ResBuf<Material>* buf, std::string name,
                       double throughput);
  MatlSellPolicy& Init(Agent* manager, ResBuf<Material>* buf, std::string name,
                       bool ignore_comp);
  MatlSellPolicy& Init(Agent* manager, ResBuf<Material>* buf, std::string name,
                       double throughput, bool ignore_comp);
  MatlSellPolicy& Init(
      Agent* manager, ResBuf<Material>* buf, std::string name,
      double throughput, bool ignore_comp, double quantize,
      std::string package_name = Package::unpackaged_name(),
      std::string transport_unit_name = TransportUnit::unrestricted_name(),
      bool discrete = false);
  /// @}

  /// Instructs the policy to empty its buffer with offers on the given
  /// commodity.  This must be called at least once or the policy will do
  /// nothing.  The policy can offer on an arbitrary number of commodities by
  /// calling Set multiple times.
  MatlSellPolicy& Set(std::string commod);

  /// Registers this policy as a trader in the current simulation.  This
  /// function must be called for the policy to begin participating in resource
  /// exchange. Init MUST be called prior to calling this function.  Start is
  /// idempotent.
  void Start();

  /// Unregisters this policy as a trader in the current simulation. This
  /// function need only be called if a policy is to be stopped *during* a
  /// simulation (it is not required to be called explicitly at the end). Stop
  /// is idempotent.
  void Stop();

  /// the current (total) limit on transactions, i.e., the quantity of resources
  /// that can be transacted in a time step
  double Limit() const;

  /// whether trades will be denoted as exclusive or not
  inline bool Excl() const { return quantize_ > 0 || discrete_; }

  /// whether resource objects are traded whole (see Init)
  inline bool Discrete() const { return discrete_; }

  /// Trader Methods
  /// @{
  virtual std::set<BidPortfolio<Material>::Ptr> GetMatlBids(
      CommodMap<Material>::type& commod_requests);
  virtual void GetMatlTrades(
      const std::vector<Trade<Material>>& trades,
      std::vector<std::pair<Trade<Material>, Material::Ptr>>& responses);
  /// }@

 private:
  void set_quantize(double x);
  void set_throughput(double x);
  void set_ignore_comp(bool x);
  void set_package(std::string x);
  void set_transport_unit(std::string x);
  void set_discrete(bool x);

  /// Discrete mode: the objects in the buffer that can be sold this time
  /// step, oldest first, skipping any object that would push the running
  /// total over the throughput or that cannot be shipped in package_.
  std::vector<Material::Ptr> DiscreteSellable() const;

  /// Discrete mode: whether obj can be traded whole. True if the buffer keeps
  /// packaging and obj is already packaged (it ships in its own package), if
  /// no sell package is set, or if obj fits in a single package_.
  bool DiscreteFits(Material::Ptr obj) const;

  /// Discrete mode: bids for each request, one BidPortfolio per object.
  std::set<BidPortfolio<Material>::Ptr> GetDiscreteBids(
      CommodMap<Material>::type& commod_requests);

  /// Discrete mode: responds to each trade with the original buffer object
  /// that its bid offered.
  void GetDiscreteTrades(
      const std::vector<Trade<Material>>& trades,
      std::vector<std::pair<Trade<Material>, Material::Ptr>>& responses);

  ResBuf<Material>* buf_;
  std::set<std::string> commods_;
  double quantize_;
  double throughput_;
  std::string name_;
  bool ignore_comp_;
  bool discrete_;
  Package::Ptr package_;
  TransportUnit::Ptr transport_unit_;

  /// Discrete mode: maps each offer made in GetMatlBids to the buffer object
  /// it represents, so GetMatlTrades can hand over that exact object.
  std::map<Material::Ptr, Material::Ptr> offer_to_obj_;
};

}  // namespace toolkit
}  // namespace cyclus

#endif
