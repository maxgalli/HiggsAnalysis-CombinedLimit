#ifndef ROO_BERNSTEINFAST
#define ROO_BERNSTEINFAST

#include "RooBernstein.h"
#include "RooListProxy.h"
#include "RooRealConstant.h"
#include "RooAbsReal.h"
#include "TClass.h"
#include "TDataMember.h"

template<int N> class RooBernsteinFast : public RooBernstein {
public:

  RooBernsteinFast() = default;

  RooBernsteinFast(const char *name, const char *title,
                   RooAbsReal& x, const RooArgList& coefList) :
    RooBernstein(name, title,
                 dynamic_cast<RooAbsRealLValue&>(x),
                 buildFullList(coefList))
  {}

  ClassDefOverride(RooBernsteinFast, 2)

private:

  // RooBernstein expects N+1 coefficients; RooBernsteinFast fixed c0=1 internally,
  // so we prepend a constant 1.0 to the user-supplied N coefficients.
  static RooArgList buildFullList(const RooArgList& coefList) {
    RooArgList full;
    full.add(RooRealConstant::value(1.0));
    full.add(coefList);
    return full;
  }

};

// Custom Streamer: handles reading old (v1) files where RooBernsteinFast
// inherited from RooAbsPdf and stored N coefficients (c0=1 was implicit).
template<int N>
void RooBernsteinFast<N>::Streamer(TBuffer &R__b) {
  if (R__b.IsReading()) {
    UInt_t R__s, R__c;
    Version_t R__v = R__b.ReadVersion(&R__s, &R__c);
    R__b.ReadClassBuffer(RooBernsteinFast::Class(), this, R__v, R__s, R__c);

    if (R__v < 2) {
      // Old version stored N coefficients (c0=1 was implicit).
      // RooBernstein expects N+1 explicit coefficients. Prepend 1.0.
      TClass *cl = TClass::GetClass("RooBernstein");
      TDataMember *dm = cl ? cl->GetDataMember("_coefList") : nullptr;
      if (dm) {
        auto &proxy = *reinterpret_cast<RooListProxy *>(
            reinterpret_cast<char *>(static_cast<RooBernstein *>(this)) + dm->GetOffset());

        if (proxy.size() == static_cast<std::size_t>(N)) {
          RooArgList updated;
          updated.add(RooRealConstant::value(1.0));
          updated.add(proxy);
          proxy.removeAll();
          proxy.add(updated);
        }
      }
    }
  } else {
    R__b.WriteClassBuffer(RooBernsteinFast::Class(), this);
  }
}

#endif
