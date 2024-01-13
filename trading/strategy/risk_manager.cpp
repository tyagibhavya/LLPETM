#include "risk_manager.h"

#include "order_manager.h"

/* RiskManager constructor expects a Logger object, a pointer to a PositionKeeper object, and a reference
to an object of the TradeEngineCfgHashMap type (ticker_cfg) that holds the risk configurations. 
It initializes the logger_ member variable and stores the PositionInfo objects from the PositionKeeper (getPositionInfo()) 
and RiskCfg objects from TradeEngineCfgHashMap (risk_cfg_) in the TickerRiskInfoHashMap data member (ticker_risk_ */

namespace Trading {
  RiskManager::RiskManager(Common::Logger *logger, const PositionKeeper *position_keeper, const TradeEngineCfgHashMap &ticker_cfg)
      : logger_(logger) {
    for (TickerId i = 0; i < ME_MAX_TICKERS; ++i) {
      ticker_risk_.at(i).position_info_ = position_keeper->getPositionInfo(i);
      ticker_risk_.at(i).risk_cfg_ = ticker_cfg[i].risk_cfg_;
    }
  }
}
