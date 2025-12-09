#!/usr/bin/env python3

import re
from optparse import OptionParser
from sys import argv, exit, modules, stderr, stdout

import ROOT
from HiggsAnalysis.CombinedLimit.DatacardParser import *
from HiggsAnalysis.CombinedLimit.ModelTools import *
from HiggsAnalysis.CombinedLimit.PhysicsModel import *
from HiggsAnalysis.CombinedLimit.ShapeTools import *
from HiggsAnalysis.CombinedLimit.TimingProfiler import get_profiler, print_timing_summary

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
else:
    file = open(options.fileName)

## Parse text file
profiler = get_profiler()

@profiler.time_function("text2workspace.parseCard")
def timed_parseCard():
    return parseCard(file, options)

DC = timed_parseCard()

if options.dumpCard:
    DC.print_structure()
    exit()

## Load tools to build workspace
@profiler.time_function("text2workspace.createModelBuilder")
def timed_createModelBuilder():
    if DC.hasShapes:
        return ShapeBuilder(DC, options)
    else:
        return CountingModelBuilder(DC, options)

MB = timed_createModelBuilder()

## Load physics model
@profiler.time_function("text2workspace.loadPhysicsModel")
def timed_loadPhysicsModel():
    (physModMod, physModName) = options.physModel.split(":")
    __import__(physModMod)
    mod = modules[physModMod]
    physics = getattr(mod, physModName)
    if mod == None:
        raise RuntimeError("Physics model module %s not found" % physModMod)
    if physics == None or not isinstance(physics, PhysicsModelBase):
        raise RuntimeError(f"Physics model {physModName} in module {physModMod} not found, or not inheriting from PhysicsModelBase")
    physics.setPhysicsOptions(options.physOpt)
    return physics

physics = timed_loadPhysicsModel()

## Attach to the tools, and run
@profiler.time_function("text2workspace.setPhysicsAndDoModel")
def timed_doModel():
    MB.setPhysics(physics)
    MB.doModel(justCheckPhysicsModel=options.justCheckPhysicsModel)

timed_doModel()

## Print timing summary
print_timing_summary(min_time=0.001)
