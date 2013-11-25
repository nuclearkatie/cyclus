// transaction.h
// conveys offer/request transaction info via inter-model Messages
#ifndef CYCLUS_TRANSACTION_H_
#define CYCLUS_TRANSACTION_H_

#include <string>

namespace cyclus {

class Context;
  
class Transaction {
 public:
  Transaction(Context* ctx, int sender_id, int receiver_id, int rsrc_id,
              std::string commodity, double price = 0.0);

  /// @return the id of the sender
  inline int sender_id() const {return sender_id_;}
  
  /// @return the id of the receiver
  inline int receiver_id() const {return receiver_id_;}

  /// @return the id of the resource
  inline int resource_id() const {return resource_id_;}
  
  /// @return the commodity requested or offered in this transaction.
  inline std::string commodity() const {return commod_;}

  /// @return the price (in dollars) being requested or offered in this
  /// transaction.
  inline double price() const {return price_;}

  /// @brief records the transaction with any backends the context knows about
  void Record();
  
  /* -------------------- private methods and members -------------------------- */
  Context* ctx_;
  std::string commod_;
  double price_;
  int id_, sender_id_, receiver_id_, resource_id_;
  static int next_id_;
};

} // namespace cyclus

#endif // CYCLUS_TRANSACTION_H_

