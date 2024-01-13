#pragma once

#include "common/macros.h"
#include "common/logging.h"

#include "position_keeper.h"
#include "om_order.h"

using namespace Common;

namespace Trading {
  class OrderManager;

  /// Enumeration that captures the result of a risk check - ALLOWED means it passed all risk checks, the other values represent the failure reason.

/* 
(1) INVALID represents an invalid sentinel value.

(2) ORDER_TOO_LARGE means that the risk check failed because the order quantity that we are 
    attempting to send would exceed the maximum allowed order quantity limit.

(3) POSITION_TOO_LARGE means that the current position, plus the order quantity on the side we are attempting 
    to send, would cause us to potentially exceed the maximum position limit that’s been configured in RiskManager.

(4) LOSS_TOO_LARGE enumeration represents the fact that the risk check failed because the trading strategy’s
    total loss (realized plus unrealized loss) is above what is allowed in RiskManager.

(5) ALLOWED enumeration is a value that represents that all risk checks passed successfully.
*/

enum class RiskCheckResult : int8_t {
    INVALID = 0,
    ORDER_TOO_LARGE = 1,
    POSITION_TOO_LARGE = 2,
    LOSS_TOO_LARGE = 3,
    ALLOWED = 4
  };

  inline auto riskCheckResultToString(RiskCheckResult result) {
    switch (result) {
      case RiskCheckResult::INVALID:
        return "INVALID";
      case RiskCheckResult::ORDER_TOO_LARGE:
        return "ORDER_TOO_LARGE";
      case RiskCheckResult::POSITION_TOO_LARGE:
        return "POSITION_TOO_LARGE";
      case RiskCheckResult::LOSS_TOO_LARGE:
        return "LOSS_TOO_LARGE";
      case RiskCheckResult::ALLOWED:
        return "ALLOWED";
    }

    return "";
  }

  /// Structure that represents the information needed for risk checks for a single trading instrument.
  struct RiskInfo {
/*  const pointer to PositionInfo called position_info_. 
This will be used to fetch the position and PnL information for the trading instrument. */
    const PositionInfo *position_info_ = nullptr;

    RiskCfg risk_cfg_;

    /// Check risk to see if we are allowed to send an order of the specified quantity on the specified side.
    /// Will return a RiskCheckResult value to convey the output of the risk check.
    auto checkPreTradeRisk(Side side, Qty qty) const noexcept {
      // check order-size
      if (UNLIKELY(qty > risk_cfg_.max_order_size_))
        return RiskCheckResult::ORDER_TOO_LARGE;
      if (UNLIKELY(std::abs(position_info_->position_ + sideToValue(side) * static_cast<int32_t>(qty)) > static_cast<int32_t>(risk_cfg_.max_position_)))
        return RiskCheckResult::POSITION_TOO_LARGE;
      if (UNLIKELY(position_info_->total_pnl_ < risk_cfg_.max_loss_))
        return RiskCheckResult::LOSS_TOO_LARGE;

      return RiskCheckResult::ALLOWED;
    }

/* Added a toString() method to the structure for logging purposes */
    auto toString() const {
      std::stringstream ss;
      ss << "RiskInfo" << "["
         << "pos:" << position_info_->toString() << " "
         << risk_cfg_.toString()
         << "]";

      return ss.str();
    }
  };

  /// Hash map from TickerId -> RiskInfo.
/*  TickerRiskInfoHashMap type, which is a std::array of RiskInfo objects of ME_MAX_TICKERS size. 
We will use this as a hash map of TickerId to RiskInfo object */
  typedef std::array<RiskInfo, ME_MAX_TICKERS> TickerRiskInfoHashMap;

  /// Top level risk manager class to compute and check risk across all trading instruments.
  class RiskManager {
  public:
    RiskManager(Common::Logger *logger, const PositionKeeper *position_keeper, const TradeEngineCfgHashMap &ticker_cfg);

    auto checkPreTradeRisk(TickerId ticker_id, Side side, Qty qty) const noexcept {
      return ticker_risk_.at(ticker_id).checkPreTradeRisk(side, qty);
    }

    /// Deleted default, copy & move constructors and assignment-operators.
    RiskManager() = delete;

    RiskManager(const RiskManager &) = delete;

    RiskManager(const RiskManager &&) = delete;

    RiskManager &operator=(const RiskManager &) = delete;

    RiskManager &operator=(const RiskManager &&) = delete;

  private:
    std::string time_str_;
    Common::Logger *logger_ = nullptr;

    /// Hash map container from TickerId -> RiskInfo.
    TickerRiskInfoHashMap ticker_risk_;
  };
}
