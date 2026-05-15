#ifndef ROO_BERNSTEINFAST
#define ROO_BERNSTEINFAST

#include "RooBernstein.h"
#include "RooRealConstant.h"
#include "RooAbsReal.h"

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

  // Schema evolution helper: migrate a v1 object (which had its own _x and
  // _coefList with N elements) into the v2 layout (inherits RooBernstein,
  // _coefList has N+1 elements with c0=1 prepended).
  // Called from <read> rules in classes_def.xml.
  void migrateFromV1(RooAbsRealLValue& x, const RooListProxy& oldCoefs) {
    // --- locate private members via TClass reflection ---
    static TClass* bernCls = TClass::GetClass("RooBernstein");
    static Long_t  coefOff = bernCls->GetDataMemberOffset("_coefList");
    static Long_t  xOff    = bernCls->GetDataMemberOffset("_x");

    static TClass* apCls    = TClass::GetClass("RooArgProxy");
    static Long_t  ownerOff = apCls->GetDataMemberOffset("_owner");
    static Long_t  argOff   = apCls->GetDataMemberOffset("_arg");
    static Long_t  vsOff    = apCls->GetDataMemberOffset("_valueServer");
    static Long_t  ssOff    = apCls->GetDataMemberOffset("_shapeServer");

    auto* base = reinterpret_cast<char*>(static_cast<RooBernstein*>(this));

    // --- _coefList (RooListProxy, private in RooBernstein) ---
    auto& coefProxy = *reinterpret_cast<RooListProxy*>(base + coefOff);
    // initializeAfterIOConstructor sets _owner, copies server-propagation
    // flags, name, and adds elements via the base-class add() (no addServer
    // calls — correct for IO since the server list was already read).
    coefProxy.initializeAfterIOConstructor(this, oldCoefs);
    // coefProxy = [c1 .. cN].  v2 needs [1.0, c1 .. cN].
    // Use base-class casts to avoid removeServer/addServer side effects.
    RooArgList tmp;
    tmp.add(coefProxy);
    static_cast<RooArgList&>(coefProxy).removeAll();
    static_cast<RooArgList&>(coefProxy).add(RooRealConstant::value(1.0));
    static_cast<RooArgList&>(coefProxy).add(tmp);

    // --- _x (RooTemplateProxy<RooAbsRealLValue>, private in RooBernstein) ---
    // RooTemplateProxy has no initializeAfterIOConstructor, so we poke the
    // underlying RooArgProxy fields directly.
    auto* xBase = reinterpret_cast<RooArgProxy*>(base + xOff);
    *reinterpret_cast<RooAbsArg**>(reinterpret_cast<char*>(xBase) + ownerOff) = this;
    *reinterpret_cast<RooAbsArg**>(reinterpret_cast<char*>(xBase) + argOff)   = &x;
    *reinterpret_cast<bool*>(reinterpret_cast<char*>(xBase) + vsOff)          = true;
    *reinterpret_cast<bool*>(reinterpret_cast<char*>(xBase) + ssOff)          = false;
    xBase->SetName("x");
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
