#include "../interface/ProfiledLikelihoodRatioTestStatExt.h"
#include "../interface/CloseCoutSentry.h"
#include "../interface/utils.h"
#include <stdexcept>
#include <RooRealVar.h>
#include <RooMinimizer.h>
#include <RooFitResult.h>
#include <RooSimultaneous.h>
#include <RooCategory.h>
#include <Math/MinimizerOptions.h>
#include <RooStats/RooStatsUtils.h>

#if 0
#define DBG(X,Z) if (X) { Z; }
#define DBGV(X,Z) if (X>1) { Z; }
#define DBG_TestStat_params 0 // ProfiledLikelihoodRatioTestStatOpt::Evaluate; 1 = dump nlls; 2 = dump params at each eval
#define DBG_TestStat_NOFIT  0 // FIXME HACK: if set, don't profile the likelihood, just evaluate it
#define DBG_PLTestStat_ctor 1 // dump parameters in c-tor
#define DBG_PLTestStat_pars 1 // dump parameters in eval
#define DBG_PLTestStat_fit  1 // dump fit result
#else
#define DBG(X,Z) 
#define DBGV(X,Z) 
#endif

//============================================================================================================================================
ProfiledLikelihoodRatioTestStatOpt::ProfiledLikelihoodRatioTestStatOpt(
    const RooArgSet & observables,
    RooAbsPdf &pdfNull, RooAbsPdf &pdfAlt,
    const RooArgSet *nuisances,
    const RooArgSet & paramsNull, const RooArgSet & paramsAlt,
    int verbosity)
: 
    pdfNull_(&pdfNull), pdfAlt_(&pdfAlt),
    paramsNull_(pdfNull_->getVariables()), 
    paramsAlt_(pdfAlt_->getVariables()), 
    verbosity_(verbosity)
{
    snapNull_.addClone(paramsNull);
    snapAlt_.addClone(paramsAlt);
    if (nuisances) nuisances_.addClone(*nuisances);
}

Double_t ProfiledLikelihoodRatioTestStatOpt::Evaluate(RooAbsData& data, RooArgSet& nullPOI)
{
    *paramsNull_ = nuisances_;
    *paramsNull_ = snapNull_;
    *paramsNull_ = nullPOI;
        
    DBGV(DBG_TestStat_params, (std::cout << "Parameters of null pdf (pre fit)" << pdfNull_->GetName()))
    DBGV(DBG_TestStat_params, (paramsNull_->Print("V")))
    double nullNLL = minNLL(*pdfNull_, data);
    
    *paramsAlt_ = nuisances_;
    *paramsAlt_ = snapAlt_;
        
    DBGV(DBG_TestStat_params, (std::cout << "Parameters of alt pdf " << pdfAlt_->GetName()))
    DBGV(DBG_TestStat_params, (paramsAlt_->Print("V")))
    double altNLL = minNLL(*pdfAlt_, data);
    
    DBG(DBG_TestStat_params, (printf("Pln: null = %+8.4f, alt = %+8.4f\n", nullNLL, altNLL)))
    return nullNLL-altNLL;
}

double ProfiledLikelihoodRatioTestStatOpt::minNLL(RooAbsPdf &pdf, RooAbsData &data) 
{
    if (verbosity_ > 0) std::cout << "Profiling likelihood for pdf " << pdf.GetName() << std::endl;
    std::auto_ptr<RooAbsReal> nll_(pdf.createNLL(data, RooFit::Constrain(nuisances_)));
#if defined(DBG_TestStat_NOFIT) && (DBG_TestStat_NOFIT > 0)
    return nll_->getVal();
#endif
    RooMinimizer minim(*nll_);
    minim.setStrategy(0);
    minim.setPrintLevel(verbosity_-1);
    minim.minimize(ROOT::Math::MinimizerOptions::DefaultMinimizerType().c_str(), ROOT::Math::MinimizerOptions::DefaultMinimizerAlgo().c_str());
    if (verbosity_ > 1) {
        std::auto_ptr<RooFitResult> res(minim.save());
        res->Print("V");
    }
    return nll_->getVal();
}

//============================================================================================================================================
ProfiledLikelihoodTestStatOpt::ProfiledLikelihoodTestStatOpt(
    const RooArgSet & observables,
    RooAbsPdf &pdf, 
    const RooArgSet *nuisances, 
    const RooArgSet & params,
    const RooArgList &gobsParams,
    const RooArgList &gobs,
    int verbosity)
:
    pdf_(&pdf),
    gobsParams_(gobsParams),
    gobs_(gobs),
    verbosity_(verbosity)
{
    params.snapshot(snap_,false);
    ((RooRealVar*)snap_.find(params.first()->GetName()))->setConstant(false);
    poi_.add(snap_);
    if (nuisances) { nuisances_.add(*nuisances); snap_.addClone(*nuisances, /*silent=*/true); }
    params_.reset(pdf_->getParameters(observables));
    DBG(DBG_PLTestStat_ctor, (std::cout << "Observables: " << std::endl)) DBG(DBG_PLTestStat_ctor, (observables.Print("V")))
    DBG(DBG_PLTestStat_ctor, (std::cout << "All params: " << std::endl))  DBG(DBG_PLTestStat_ctor, (params_->Print("V")))
    DBG(DBG_PLTestStat_ctor, (std::cout << "Snapshot: " << std::endl))    DBG(DBG_PLTestStat_ctor, (snap_.Print("V")))
    DBG(DBG_PLTestStat_ctor, (std::cout << "POI: " << std::endl))         DBG(DBG_PLTestStat_ctor, (poi_.Print("V")))
    
}


Double_t ProfiledLikelihoodTestStatOpt::Evaluate(RooAbsData& data, RooArgSet& /*nullPOI*/)
{
    // Take snapshot of initial state, to restore it at the end 
    RooArgSet initialState; params_->snapshot(initialState);

    DBG(DBG_PLTestStat_pars, std::cout << "Being evaluated on " << data.GetName() << ": params before snapshot are " << std::endl)
    DBG(DBG_PLTestStat_pars, params_->Print("V"))
    // Initialize parameters
    *params_ = snap_;

    //set value of "globalConstrained" nuisances to the value of the corresponding global observable and set them constant
    for (int i=0; i<gobsParams_.getSize(); ++i) {
      RooRealVar *nuis = (RooRealVar*)gobsParams_.at(i);
      RooRealVar *gobs = (RooRealVar*)gobs_.at(i);
      nuis->setVal(gobs->getVal());
      nuis->setConstant();
    }
        
    DBG(DBG_PLTestStat_pars, std::cout << "Being evaluated on " << data.GetName() << ": params after snapshot are " << std::endl)
    DBG(DBG_PLTestStat_pars, params_->Print("V"))
    // Initialize signal strength
    RooRealVar *rIn = (RooRealVar *) poi_.first();
    RooRealVar *r   = (RooRealVar *) params_->find(rIn->GetName());
    r->setMin(0); if (rIn->getVal() == 0) r->removeMax(); else r->setMax(rIn->getVal());
    r->setConstant(false);
    DBG(DBG_PLTestStat_pars, (std::cout << "r In: ")) DBG(DBG_PLTestStat_pars, (rIn->Print(""))) DBG(DBG_PLTestStat_pars, std::cout << std::endl)
    DBG(DBG_PLTestStat_pars, std::cout << "r before the fit: ") DBG(DBG_PLTestStat_pars, r->Print("")) DBG(DBG_PLTestStat_pars, std::cout << std::endl)

    // Perform unconstrained minimization (denominator)
    double nullNLL = minNLL(*pdf_, data);
    DBG(DBG_PLTestStat_pars, (std::cout << "r after the fit: ")) DBG(DBG_PLTestStat_pars, (r->Print(""))) DBG(DBG_PLTestStat_pars, std::cout << std::endl)

    // Perform unconstrained minimization (numerator)
    r->setVal(rIn->getVal()); 
    r->setConstant(true);
    double thisNLL = minNLL(*pdf_, data);
    
    DBG(DBG_PLTestStat_pars, std::cout << "Was evaluated on " << data.GetName() << ": params before snapshot are " << std::endl)
    DBG(DBG_PLTestStat_pars, params_->Print("V"))

    //Restore initial state, to avoid issues with ToyMCSampler
    *params_ = initialState;

    return thisNLL-nullNLL;
}

double ProfiledLikelihoodTestStatOpt::minNLL(RooAbsPdf &pdf, RooAbsData &data) 
{

    std::auto_ptr<RooAbsReal> nll(pdf.createNLL(data, RooFit::Constrain(nuisances_)));
    RooMinimizer minim(*nll);
    minim.setStrategy(0);
    minim.setPrintLevel(verbosity_-1);    
    minim.minimize(ROOT::Math::MinimizerOptions::DefaultMinimizerType().c_str(), ROOT::Math::MinimizerOptions::DefaultMinimizerAlgo().c_str());
    if (verbosity_ > 1) {
        minim.hesse();
        std::auto_ptr<RooFitResult> res(minim.save());
        res->Print("V");
    }
  
    return nll->getVal();
    
}

