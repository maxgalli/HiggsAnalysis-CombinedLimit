#ifndef ROO_BERNSTEINFAST
#define ROO_BERNSTEINFAST

#include "RooBernstein.h"
#include "RooRealConstant.h"
#include "RooAbsReal.h"

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
