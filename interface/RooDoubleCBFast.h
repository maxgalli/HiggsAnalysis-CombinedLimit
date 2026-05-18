#ifndef ROODOUBLECB
#define ROODOUBLECB

#include "RooCrystalBall.h"
#include "RooAbsReal.h"

class RooDoubleCBFast : public RooCrystalBall {
public:
  RooDoubleCBFast() = default;
  RooDoubleCBFast(const char *name, const char *title,
              RooAbsReal& _x,
              RooAbsReal& _mean,
              RooAbsReal& _width,
              RooAbsReal& _alpha1,
              RooAbsReal& _n1,
              RooAbsReal& _alpha2,
              RooAbsReal& _n2
           );

  // In-place migration from v1 (RooAbsPdf base) to v2 (RooCrystalBall base).
  // Uses TClass reflection to set RooCrystalBall's private proxy members
  // without calling the destructor, which would corrupt ROOT's TProcessID state.
  void migrateFromV1(RooAbsReal& x, RooAbsReal& mean, RooAbsReal& width,
                     RooAbsReal& alpha1, RooAbsReal& n1,
                     RooAbsReal& alpha2, RooAbsReal& n2);

  ClassDefOverride(RooDoubleCBFast, 2)
};
#endif
