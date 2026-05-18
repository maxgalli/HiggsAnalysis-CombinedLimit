#include "../interface/RooDoubleCBFast.h"
#include "RooAbsReal.h"
#include "RooAbsPdf.h"
#include "RooRealProxy.h"
#include "RooArgProxy.h"
#include "TClass.h"
#include "TRefArray.h"

using namespace RooFit;

 ClassImp(RooDoubleCBFast)

 RooDoubleCBFast::RooDoubleCBFast(const char *name, const char *title,
                    RooAbsReal& _x,
                    RooAbsReal& _mean,
                    RooAbsReal& _width,
                    RooAbsReal& _alpha1,
                    RooAbsReal& _n1,
                    RooAbsReal& _alpha2,
                    RooAbsReal& _n2
                    ) :
   RooCrystalBall(name, title, _x, _mean, _width, _alpha1, _n1, _alpha2, _n2)
 {
 }

void RooDoubleCBFast::migrateFromV1(RooAbsReal& x, RooAbsReal& mean, RooAbsReal& width,
                                     RooAbsReal& alpha1, RooAbsReal& n1,
                                     RooAbsReal& alpha2, RooAbsReal& n2) {
    auto* cbClass = TClass::GetClass("RooCrystalBall");
    auto* proxyClass = TClass::GetClass("RooArgProxy");

    char* base = reinterpret_cast<char*>(this);

    // RooArgProxy field offsets
    auto ownerOff = proxyClass->GetDataMemberOffset("_owner");
    auto argOff = proxyClass->GetDataMemberOffset("_arg");
    auto valServOff = proxyClass->GetDataMemberOffset("_valueServer");
    auto shpServOff = proxyClass->GetDataMemberOffset("_shapeServer");
    auto isFundOff = proxyClass->GetDataMemberOffset("_isFund");

    // Set up an inline RooRealProxy: poke its RooArgProxy fields directly.
    // The proxy was default-constructed (_owner=nullptr, _arg=nullptr) so
    // we fill the fields without triggering registerProxy/addServer.
    auto setupProxy = [&](const char* member, const char* pName, const char* pTitle, RooAbsReal& arg) {
        char* p = base + cbClass->GetDataMemberOffset(member);
        reinterpret_cast<TNamed*>(p)->SetNameTitle(pName, pTitle);
        *reinterpret_cast<RooAbsArg**>(p + ownerOff) = this;
        *reinterpret_cast<RooAbsArg**>(p + argOff) = &arg;
        *reinterpret_cast<bool*>(p + valServOff) = true;
        *reinterpret_cast<bool*>(p + shpServOff) = false;
        *reinterpret_cast<bool*>(p + isFundOff) = arg.isFundamental();
        _proxyList.Add(reinterpret_cast<TObject*>(p));
    };

    setupProxy("x_", "x", "Dependent", x);
    setupProxy("x0_", "x0", "X0", mean);
    setupProxy("sigmaL_", "sigmaL", "Left Sigma", width);
    setupProxy("sigmaR_", "sigmaR", "Right Sigma", width);
    setupProxy("alphaL_", "alphaL", "Left Alpha", alpha1);
    setupProxy("nL_", "nL", "Left Order", n1);

    // For unique_ptr<RooRealProxy> members: allocate on the heap,
    // poke fields, then assign to the unique_ptr.
    auto setupUPtr = [&](const char* member, const char* pName, const char* pTitle, RooAbsReal& arg) {
        auto* uptr = reinterpret_cast<std::unique_ptr<RooRealProxy>*>(
            base + cbClass->GetDataMemberOffset(member));
        auto* proxy = new RooRealProxy();
        char* p = reinterpret_cast<char*>(static_cast<RooArgProxy*>(proxy));
        proxy->SetNameTitle(pName, pTitle);
        *reinterpret_cast<RooAbsArg**>(p + ownerOff) = this;
        *reinterpret_cast<RooAbsArg**>(p + argOff) = &arg;
        *reinterpret_cast<bool*>(p + valServOff) = true;
        *reinterpret_cast<bool*>(p + shpServOff) = false;
        *reinterpret_cast<bool*>(p + isFundOff) = arg.isFundamental();
        _proxyList.Add(static_cast<TObject*>(proxy));
        uptr->reset(proxy);
    };

    setupUPtr("alphaR_", "alphaR", "Right Alpha", alpha2);
    setupUPtr("nR_", "nR", "Right Order", n2);

    // Replace the stale ioEvoList entry (contains TRefs to v1 proxies that
    // don't exist in the v2 layout) with an empty one. This prevents
    // ioStreamerPass2Finalize from crashing on unresolvable TRefs.
    RooAbsArg::addToIoEvoList(this, TRefArray());
}

