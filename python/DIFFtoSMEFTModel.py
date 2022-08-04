"""

"""
from HiggsAnalysis.CombinedLimit.PhysicsModel import *
from HiggsAnalysis.CombinedLimit.SMHiggsBuilder import SMHiggsBuilder

import ROOT
import yaml
import json
import os

edges = {
    "hgg": { # see https://github.com/maxgalli/EFT2Obs/blob/WithHiggsDecay/RivetPlugins/Higgs2GGFiducialAndDifferential.cc
        "pt": [0.0 ,5.0 , 10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 45.0, 60.0, 80.0, 100.0, 120.0, 140.0, 170.0, 200.0, 250.0, 350.0, 450.0, 10000.0]
    }
}

decay_file_conversions = {
    "gamgam": "hgg"
}


class DIFFtoSMEFTModel(PhysicsModel):
    def __init__(self):
        self.higgs_mass = 125.38
        self.linear_only = False
        super(DIFFtoSMEFTModel, self).__init__()


    def make_scaling_function(self, name, terms):
        coeffs = ROOT.std.map("string", "double")()
        for jpoi_name in self.pois_info.keys():
            if "A_{}".format(jpoi_name) in terms: 
                coeffs[jpoi_name] = terms["A_{}".format(jpoi_name)]

            if not self.linear_only:
                if "B_{}_2".format(jpoi_name) in terms: 
                    coeffs["{}_2".format(jpoi_name)] = terms["B_{}_2".format(jpoi_name)]
                for kpoi_name in self.pois_info.keys(): 
                    if "B_{}_{}".format(jpoi_name, kpoi_name) in terms:
                        coeffs["{}_{}".format(jpoi_name, kpoi_name)] = terms["B_{}_{}".format(jpoi_name, kpoi_name)]
        
        roo_name = "scaling_{}".format(name)
        print("Making scaling function {}".format(roo_name))
        eft_scaling = ROOT.RooEFTScalingFunction(roo_name, roo_name, coeffs, self.POIs)

        # sanity check
        #for coeff in coeffs:
            #print("{} = {}".format(coeff.first, coeff.second))

        self.modelBuilder.out._import(eft_scaling)
    
    
    def setPhysicsOptions(self, physOptions):
        """ Some important random notes:
        - input_dir points to a directory where yaml and json files specifying the model can be found
        """
        if not any(po.startswith("input_dir=") for po in physOptions):
            raise RuntimeError("Please specify the input directory with --PO 'input_dir=...'")

        for po in physOptions:
            if po.startswith("input_dir="):
                self.input_dir = po.replace("input_dir=", "")
                print "input_dir:", self.input_dir
            
            if po == "linear_only":
                self.linear_only = True

            if po.startswith("config_file="):
                self.config_file = po.replace("config_file=", "")
                print "config_file:", self.config_file


    def doParametersOfInterest(self):
        # set mass
        # no idea why we need to do this if-else
        if self.modelBuilder.out.var("MH"):
            self.modelBuilder.out.var("MH").setVal(self.higgs_mass)
            self.modelBuilder.out.var("MH").setConstant(True)
        else:
            self.modelBuilder.doVar("MH[%g]" % self.higgs_mass)

        # read the yaml file for POIs
        with open(self.config_file, "r") as f:
            self.pois_info = yaml.safe_load(f)

        # set Wilson coefficients
        poi_names = []
        for name, info in self.pois_info.items():
            poi_names.append(name)
            self.modelBuilder.doVar("%s[%g,%g,%g]"%(name, info['val'], info['min'], info['max']))
            self.modelBuilder.out.var(name).setConstant(True)
        
        self.modelBuilder.doSet("POI", ",".join(poi_names))
        self.POIs = ROOT.RooArgList(self.modelBuilder.out.set("POI"))

        # Create scaling functions and add them to the model
        # Both production and decay
        """
        production_terms = {
            "hgg": {
                "0p0_5p0": {
                    "A_cdp": ...,
                }
            }
        }
        """
        production_terms = {}
        for decay_channel_dir in os.listdir(os.path.join(self.input_dir, "differentials")):
            production_terms[decay_channel_dir] = {}
            with open(os.path.join(self.input_dir, "differentials", decay_channel_dir, "ggH_SMEFTatNLO_pt_gg.json"), "r") as f:
                tmp_dct = json.load(f)
                for edge, next_edge in zip(edges[decay_channel_dir]['pt'][:-1], edges[decay_channel_dir]['pt'][1:]):
                    production_terms[decay_channel_dir]["{}_{}".format(str(edge).replace(".", "p"), str(next_edge).replace(".", "p"))] = tmp_dct[str(edge)]

        with open("{}/decay.json".format(self.input_dir), "r") as f:
            decay_terms = json.load(f)

        # First add the decay widths scaling function (partial and total are in the same file)
        # tot before everything else since BR construcion depends on it
        self.make_scaling_function("tot", decay_terms)

        for mode in decay_terms:
            if mode != "tot" and mode in decay_file_conversions:
                mode_conv_name = decay_file_conversions[mode]
                self.make_scaling_function("partial_{}".format(mode_conv_name), decay_terms[mode])
                
                # And we make the BR by making the ratio with scaling_tot
                print("Making scaling function scaling_BR_{}".format(mode_conv_name))
                self.modelBuilder.factory_('expr::scaling_BR_{}("@0/@1", scaling_partial_{}, scaling_tot)'.format(mode_conv_name, mode_conv_name))
        
        # Now production (a bit more complicated)
        self.full_scaling_names = []
        self.channels = []
        self.bin_ranges = []
        for production_mode in production_terms: # e.g. hgg
            if production_mode not in self.channels:
                self.channels.append(production_mode)
            for bin_range in production_terms[production_mode]: # e.g. 0p0_5p0
                if bin_range not in self.bin_ranges:
                    self.bin_ranges.append(bin_range)
                name = "{}_{}".format(production_mode, bin_range)
                self.make_scaling_function(name, production_terms[production_mode][bin_range])

                # And we make the full mu by multiplying the partial mu with the BR
                full_scaling_name = "full_scaling_{}_{}".format(production_mode, bin_range)
                print("Making scaling function {}".format(full_scaling_name))
                self.modelBuilder.factory_("prod::{}({})".format(full_scaling_name, ",".join(["scaling_{}".format(name), "scaling_BR_{}".format(production_mode)])))
                #self.modelBuilder.factory_('expr::{}("@0*@1", scaling_{}, scaling_BR_{})'.format(full_scaling_name, name, production_mode))
                self.full_scaling_names.append(full_scaling_name)


    def getYieldScale(self, bin, process):
        print("getYieldScale: bin={}, process={}".format(bin, process))
        if self.DC.isSignal[process] and process != "OutsideAcceptance":
            # Process name contains the bin range, e.g. 0p0_5p0
            bin_range = [bnr for bnr in self.bin_ranges if bnr in process][0]
            # Bin name contains the decay mode, e.g. hgg
            production_mode = [chn for chn in self.channels if chn in bin][0]

            full_scaling_function = [f for f in self.full_scaling_names if "{}_{}".format(production_mode, bin_range) in f][0]
            print("Scaling process {} in bin {} with function {}".format(process, bin, full_scaling_function))
            
            return full_scaling_function
        
        return 1


diff_to_smeft_model = DIFFtoSMEFTModel()
