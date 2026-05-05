#ifndef ROO_BERNSTEINFAST
#define ROO_BERNSTEINFAST

#include "RooBernstein.h"
#include "RooListProxy.h"
#include "RooRealConstant.h"
#include "RooAbsReal.h"
#include "TClass.h"
#include "TDataMember.h"

#include <TClass.h>
#include <TDataMember.h>
#include <RooArgProxy.h>
#include <RooListProxy.h>

template<int N> class RooBernsteinFast : public RooBernstein {
public:

  RooBernsteinFast() = default;

  RooBernsteinFast(const char *name, const char *title,
                   RooAbsReal& x, const RooArgList& coefList) :
    // NOTE: RooBernstein takes RooAbsRealLValue& for x, while this constructor
    // keeps RooAbsReal& for backward compatibility. The dynamic_cast will throw
    // at runtime if x is not actually a RooAbsRealLValue.
    RooBernstein(name, title,
                 dynamic_cast<RooAbsRealLValue&>(x),
                 buildFullList(coefList))
  {}

  // Called after RooFit's pass-2 proxy resolution, at which point all
  // RooAbsArg* pointers in proxies are valid. Objects read from version-1
  // files were written with N coefficients (c0=1 was implicit); RooBernstein
  // requires N+1 explicit coefficients. Fix up here.
  void ioStreamerPass2() override {
    RooBernstein::ioStreamerPass2(); // resolves all proxies in base

    // RooBernstein::_coefList is private; access it via TClass reflection.
    // RooBernstein is the only base, so its subobject starts at offset 0.
    TClass *cl = TClass::GetClass("RooBernstein");
    TDataMember *dm = cl ? cl->GetDataMember("_coefList") : nullptr;
    if (!dm) return;

    auto &proxy = *reinterpret_cast<RooListProxy *>(
        reinterpret_cast<char *>(static_cast<RooBernstein *>(this)) + dm->GetOffset());

    if (proxy.size() != static_cast<std::size_t>(N))
      return; // already N+1 (new file) or unexpected state — leave as-is

    // Rebuild the list with c0=1.0 prepended.
    RooArgList updated;
    updated.add(RooRealConstant::value(1.0));
    updated.add(proxy);
    proxy.removeAll();
    proxy.add(updated);
  }

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

#endif
