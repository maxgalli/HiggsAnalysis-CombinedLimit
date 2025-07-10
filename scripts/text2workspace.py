#!/usr/bin/env python3

import re
from optparse import OptionParser
from sys import argv, exit, modules, stderr, stdout

import ROOT
from HiggsAnalysis.CombinedLimit.DatacardParser import *
from HiggsAnalysis.CombinedLimit.ModelTools import *
from HiggsAnalysis.CombinedLimit.PhysicsModel import *
from HiggsAnalysis.CombinedLimit.ShapeTools import *

ROOT.gSystem.Load('../../build/libHiggsAnalysisCombinedLimit.so')

# import ROOT with a fix to get batch mode (http://root.cern.ch/phpBB3/viewtopic.php?t=3198)
argv.append("-b-")

ROOT.gROOT.SetBatch(True)
ROOT.PyConfig.IgnoreCommandLineOptions = True
argv.remove("-b-")


parser = OptionParser(usage="usage: %prog [options] datacard.txt -o output \nrun with --help to get list of options")
addDatacardParserOptions(parser)
parser.add_option(
    "-P",
    "--physics-model",
    dest="physModel",
    default="HiggsAnalysis.CombinedLimit.PhysicsModel:defaultModel",
    type="string",
    help="Physics model to use. It should be in the form (module name):(object name)",
)
parser.add_option(
    "--PO",
    "--physics-option",
    dest="physOpt",
    default=[],
    type="string",
    action="append",
    help="Pass a given option to the physics model (can specify multiple times)",
)
parser.add_option(
    "",
    "--dump-datacard",
    dest="dumpCard",
    default=False,
    action="store_true",
    help="Print to screen the DataCard as a python config and exit",
)
parser.add_option(
    "--just-check-physics-model",
    dest="justCheckPhysicsModel",
    default=False,
    action="store_true",
    help="Just check if the physics model is ok, without building the workspace.",
)
parser.add_option(
    "--remove-multipdf",
    dest="removeMultiPdf",
    default=False,
    action="store_true",
    help="Swap multipdf pdfs with their current index pdf",
)
(options, args) = parser.parse_args()

if len(args) == 0:
    parser.print_usage()
    exit(1)

options.fileName = args[0]
if options.fileName.endswith(".gz"):
    import gzip

    file = gzip.open(options.fileName, "rt")
    options.fileName = options.fileName[:-3]
elif options.fileName.endswith(".json"):
    import json
    with open(options.fileName, "r") as f:
        file = json.load(f)
else:
    file = open(options.fileName)

if options.fileName.endswith(".txt"):
    ## Parse text file
    DC = parseCard(file, options)

    if options.dumpCard:
        DC.print_structure()
        exit()

    ## Load tools to build workspace
    MB = None
    if DC.hasShapes:
        MB = ShapeBuilder(DC, options)
    else:
        MB = CountingModelBuilder(DC, options)

    ## Load physics model
    (physModMod, physModName) = options.physModel.split(":")
    __import__(physModMod)
    mod = modules[physModMod]
    physics = getattr(mod, physModName)
    if mod == None:
        raise RuntimeError("Physics model module %s not found" % physModMod)
    if physics == None or not isinstance(physics, PhysicsModelBase):
        raise RuntimeError(f"Physics model {physModName} in module {physModMod} not found, or not inheriting from PhysicsModelBase")
    physics.setPhysicsOptions(options.physOpt)
    ## Attach to the tools, and run
    MB.setPhysics(physics)
    MB.doModel(justCheckPhysicsModel=options.justCheckPhysicsModel)
else:
    print("It's HS3")
    w = ROOT.RooWorkspace("w")
    w.safe_import = SafeWorkspaceImporter(w)
    tool = ROOT.RooJSONFactoryWSTool(w)
    tool.importJSON(options.fileName)
    #w.Print()

    # create MH
    MH = ROOT.RooRealVar("MH", "MH", options.mass, -ROOT.RooNumber.infinity(), ROOT.RooNumber.infinity())
    MH.setConstant(True)
    w.safe_import(MH)
    #w.factory("MH[{}, -inf, inf]".format(str(options.mass)))

    # create named sets for POI, nuisances and observables, that are needed
    pois = []
    for k, v in file["misc"]["ROOT_internal"]["attributes"].items():
        tags = v["tags"]
        if "group_POI" in tags:
            pois.append(k)
    w.defineSet("POI", ",".join(pois))

    es1 = ROOT.RooArgSet()
    w.defineSet("globalObservables", es1) # THIS HAS TO BE DONE PROPERLY, PLACEHOLDER FOR NOW
    
    es2 = ROOT.RooArgSet()
    w.defineSet("nuisances", es2) # THIS HAS TO BE DONE PROPERLY, PLACEHOLDER FOR NOW

    observables = []
    for data in file["data"]:
        for ax in data["axes"]:
            name = ax["name"]
            if name not in observables:
                observables.append(name)
    w.defineSet("observables", ",".join(observables))

    #w.Print()
    mc_s = ROOT.RooStats.ModelConfig("ModelConfig", w)
    mc_b = ROOT.RooStats.ModelConfig("ModelConfig_bonly", w)
    #for l, mc in [("s", mc_s), ("b", mc_b)]: # TO BE DONE PROPERLY
    for l, mc in [("s", mc_s)]: # TO BE DONE PROPERLY
        mc.SetPdf(w.pdf("model_" + l))
        mc.SetParametersOfInterest(w.set("POI"))
        mc.SetObservables(w.set("observables"))
        nuisancesSet = ROOT.RooArgSet()
        if w.set("nuisances"):
            nuisancesSet = w.set("nuisances")
        #for nuis in self.extraNuisances:
        #    nuisancesSet.add(nuis)
        if nuisancesSet.getSize():
            mc.SetNuisanceParameters(nuisancesSet)
        gObsSet = ROOT.RooArgSet()
        if w.set("globalObservables"):
            gObsSet = w.set("globalObservables")
        #for gobs in self.extraGlobalObservables:
        #    gObsSet.add(gobs)
        if gObsSet.getSize():
            mc.SetGlobalObservables(gObsSet)
        w.safe_import(mc, mc.GetName())
        #if self.options.noBOnly:
        #    print("noBOnly")
        #    break
    discparams = ROOT.RooArgSet("discreteParams")
    #print("self.discrete_param_set")
    #print(self.discrete_param_set)
    #for cpar in self.discrete_param_set:
    #    print("cpar")
    #    print(cpar)
    #    discparams.add(self.out.cat(cpar))
    w.safe_import(discparams, discparams.GetName())
    w.Print()
    
    w.writeToFile(options.out)
