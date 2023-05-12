"""

"""
from HiggsAnalysis.CombinedLimit.PhysicsModel import *
from HiggsAnalysis.CombinedLimit.SMHiggsBuilder import SMHiggsBuilder

import ROOT
import yaml
import json
import os

edges = {
    "smH_PTH": {
        "hgg": [0.0 ,5.0 , 10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 45.0, 60.0, 80.0, 100.0, 120.0, 140.0, 170.0, 200.0, 250.0, 350.0, 450.0, 10000.0],
        "hzz": [0.0, 10.0, 20.0, 30.0, 45.0, 60.0, 80.0, 120.0, 200.0, 10000.0],
        "hww": [0.0, 30.0, 45.0, 80.0, 120.0, 200.0, 10000.0],
        "htt": [0.0, 45.0, 80.0, 120.0, 140.0, 170.0, 200.0, 350.0, 450.0, 10000.0],
        "hbbvbf": [0.0, 450.0, 500.0, 550.0, 600.0, 675.0, 800.0], # since we do not have prediction in last bin
        "httboost": [450.0, 600.0, 10000.0],
    },
    "Njets": {
        "hgg": [-0.5, 0.5, 1.5, 2.5, 3.5, 100.5],
        "hzz": [-0.5, 0.5, 1.5, 2.5, 3.5, 100.5],
        "hww": [-0.5, 0.5, 1.5, 2.5, 3.5, 100.5],
        "htt": [-0.5, 0.5, 1.5, 2.5, 3.5, 100.5],
    }
}

decay_file_conversions = {
    "gamgam": ["hgg"],
    "ZZ": ["hzz"],
    "WW": ["hww"],
    "tautau": ["htt", "httboost"],
    "bb": ["hbbvbf"],
}

max_to_matt = {
    "hgg": "gamgam",
    "hzz": "ZZ",
    "hww": "WW",
    "htt": "tautau",
    "httboost": "tautau",
    "hbbvbf": "bb"
}

ggH_production_files = {
    "smH_PTH": {
        "hgg": "ggH_SMEFTatNLO_pt_gg.json",
        "hzz": "ggH_SMEFTatNLO_pt_h.json",
        "htt": "ggH_SMEFTatNLO_pt_h.json",
        "hww": "ggH_SMEFTatNLO_pt_h.json",
        "hbbvbf": "ggH_SMEFTatNLO_pt_h.json",
        "httboost": "ggH_SMEFTatNLO_pt_h.json",
    },
    "Njets": {
        "hgg": "ggH_SMEFTatNLO_njets.json",
        "hzz": "ggH_SMEFTatNLO_njets.json",
        "hww": "ggH_SMEFTatNLO_njets.json",
        "htt": "ggH_SMEFTatNLO_njets.json",
    },
}

class DIFFtoSMEFTModel(PhysicsModel):
    def __init__(self):
        self.higgs_mass = 125.38
        self.higgs_mass_inf = 123.0
        self.higgs_mass_sup = 127.0
        self.constant_mass = False
        self.linear_only = False
        self.linearised = False
        super(DIFFtoSMEFTModel, self).__init__()


    def local_RooEFTScalingFunction(self, name, title, std_map, terms):
        expr = "1.0"
        for i, coeff in enumerate(std_map):
            expr += "+{}*@{}".format(coeff.second, i)
        print(expr)
        return ROOT.RooFormulaVar(name, title, expr, terms)


    def make_linearised_function(self, name, prod_terms, decay_terms, tot_terms, pois):
        expr = "1.0"
        for i, jpoi_name in enumerate(self.pois_info.keys()):
            if "A_{}".format(jpoi_name) in prod_terms:
                expr += "+{}*@{}".format(prod_terms["A_{}".format(jpoi_name)], i)
            else:
                expr += "+0.0*@{}".format(i)
            if "A_{}".format(jpoi_name) in decay_terms:
                expr += "+{}*@{}".format(decay_terms["A_{}".format(jpoi_name)], i)
            else:
                expr += "+0.0*@{}".format(i)
            if "A_{}".format(jpoi_name) in tot_terms:
                expr += "-{}*@{}".format(tot_terms["A_{}".format(jpoi_name)], i)
            else:
                expr += "-0.0*@{}".format(i)
        print("Making {}".format(name))
        print(expr)
        return ROOT.RooFormulaVar(name, name, expr, pois)

    def make_scaling_function(self, name, terms):
        coeffs = ROOT.std.map("string", "double")()
        print("linear only: {}".format(self.linear_only))
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
        #print("Coeffs: {}".format(coeffs))
        #print("POIs: {}".format(self.POIs))
        eft_scaling = ROOT.RooEFTScalingFunction(roo_name, roo_name, coeffs, self.POIs)
        #eft_scaling = self.local_RooEFTScalingFunction(roo_name, roo_name, coeffs, self.POIs)

        # sanity check
        for coeff in coeffs:
            print("{} = {}".format(coeff.first, coeff.second))

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

            if po == "linearised":
                self.linearised = True

            if self.linearised and self.linear_only:
                raise RuntimeError("Cannot specify linearised and linear_only at the same time!")

            if po.startswith("config_file="):
                self.config_file = po.replace("config_file=", "")
                print "config_file:", self.config_file

            if po == "constant_mass":
                self.constant_mass = True

            if po.startswith("chan_obs_file="):
                self.chan_obs_file = po.replace("chan_obs_file=", "")
                with open(self.chan_obs_file, "r") as f:
                    self.chan_obs = json.load(f)
                print "chan_obs:", self.chan_obs


    def doMH(self):
        if self.constant_mass:
            print "mass will be set constant to {}".format(self.higgs_mass)
            if self.modelBuilder.out.var("MH"):
                self.modelBuilder.out.var("MH").setVal(self.higgs_mass)
                self.modelBuilder.out.var("MH").setConstant(True)
            else:
                self.modelBuilder.doVar("MH[%g]" % self.higgs_mass)
        else:
            print "mass will be left floating between {} and {}".format(self.higgs_mass_inf, self.higgs_mass_sup)
            if self.modelBuilder.out.var("MH"):
                self.modelBuilder.out.var("MH").setRange(self.higgs_mass_inf, self.higgs_mass_sup)
                self.modelBuilder.out.var("MH").setConstant(False)
            else:
                self.modelBuilder.doVar("MH[%s,%s]" % (self.higgs_mass_inf, self.higgs_mass_sup)) 


    def doParametersOfInterest(self):
        # set mass
        self.doMH()

        # read the yaml file for POIs
        with open(self.config_file, "r") as f:
            self.pois_info = yaml.safe_load(f)
        print("self.pois_info: {}".format(self.pois_info))

        # set Wilson coefficients
        poi_names = []
        for name, info in self.pois_info.items():
            poi_names.append(name)
            self.modelBuilder.doVar("%s[%g,%g,%g]"%(name, info['val'], info['min'], info['max']))
            self.modelBuilder.out.var(name).setConstant(True)
        print("poi_names {}".format(poi_names))
        
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
        for channel, obs in self.chan_obs.items():
            production_terms[channel] = {}
            full_path_to_ggF_json = os.path.join(self.input_dir, "differentials", channel, ggH_production_files[obs][channel])
            with open(full_path_to_ggF_json, "r") as f:
                tmp_dct = json.load(f)
                for edge, next_edge in zip(edges[obs][channel][:-1], edges[obs][channel][1:]):
                    try:
                        production_terms[channel]["{}_{}".format(str(edge).replace(".", "p").replace("-", "m"), str(next_edge).replace(".", "p").replace("-", "m"))] = tmp_dct[str(edge)]
                    except KeyError:
                        print("WARNING: No differential cross section for {}-{} GeV in decay channel {}".format(edge, next_edge, channel))
                        pass
        #print("production_terms: {}".format(production_terms))

        with open("{}/decay.json".format(self.input_dir), "r") as f:
            decay_terms = json.load(f)

        self.full_scaling_names = {}
        
        if self.linearised:
            for production_mode in production_terms: # e.g. hgg
                self.full_scaling_names[production_mode] = {}
                for bin_range in production_terms[production_mode]: # e.g. 0p0_5p0
                    print(bin_range)
                    name = "full_scaling_{}_{}".format(production_mode, bin_range)
                    eq = self.make_linearised_function(name, production_terms[production_mode][bin_range], decay_terms[max_to_matt[production_mode]], decay_terms["tot"], self.POIs)
                    self.modelBuilder.out._import(eq)
                    self.full_scaling_names[production_mode][bin_range] = name
        else:
            # First add the decay widths scaling function (partial and total are in the same file)
            # tot before everything else since BR construcion depends on it
            self.make_scaling_function("tot", decay_terms["tot"])

            for mode in decay_terms:
                if mode != "tot" and mode in decay_file_conversions:
                    # for the case in which we have multiple analyses in the same decay channel
                    for mode_conv_name in decay_file_conversions[mode]:
                        self.make_scaling_function("partial_{}".format(mode_conv_name), decay_terms[mode])
                        
                        # And we make the BR by making the ratio with scaling_tot
                        print("Making scaling function scaling_BR_{}".format(mode_conv_name))
                        self.modelBuilder.factory_('expr::scaling_BR_{}("@0/@1", scaling_partial_{}, scaling_tot)'.format(mode_conv_name, mode_conv_name))
            
            # Now production (a bit more complicated)
            for production_mode in production_terms: # e.g. hgg
                self.full_scaling_names[production_mode] = {}
                for bin_range in production_terms[production_mode]: # e.g. 0p0_5p0
                    name = "{}_{}".format(production_mode, bin_range)
                    self.make_scaling_function(name, production_terms[production_mode][bin_range])

                    # And we make the full mu by multiplying the partial mu with the BR
                    full_scaling_name = "full_scaling_{}_{}".format(production_mode, bin_range)
                    print("Making scaling function {}".format(full_scaling_name))
                    self.modelBuilder.factory_("prod::{}({})".format(full_scaling_name, ",".join(["scaling_{}".format(name), "scaling_BR_{}".format(production_mode)])))
                    #self.modelBuilder.factory_('expr::{}("@0*@1", scaling_{}, scaling_BR_{})'.format(full_scaling_name, name, production_mode))
                    self.full_scaling_names[production_mode][bin_range] = full_scaling_name
        
        print("Full scaling names:")
        print(self.full_scaling_names)

    def getYieldScale(self, bin, process):
        print("getYieldScale: bin={}, process={}".format(bin, process))
        if self.DC.isSignal[process] and process != "OutsideAcceptance":
        #if self.DC.isSignal[process] and process in ["smH_PTH_30p0_35p0"]:
            try:
                # Bin name contains the decay mode, e.g. hgg
                production_mode = [chn for chn in list(self.full_scaling_names.keys()) if chn in bin][0]
                # Process name contains the bin range, e.g. 0p0_5p0
                obs = self.chan_obs[production_mode]

                if obs == "smH_PTH":
                    # Since Htt and HWW do not have p0 after the edge number
                    if production_mode in ["htt", "hww", "hbbvbf", "httboost"]:
                        if "GT450" in process: # Htt
                            bin_range = "450p0_10000p0" 
                        elif "GT200" in process: # HWW
                            bin_range = "200p0_10000p0"
                        else:
                            bin_range = [bnr for bnr in list(self.full_scaling_names[production_mode].keys()) if bnr.replace('p0', '') in process][0]
                    else:
                        # Because of convention not respected in HZZ
                        if "GT200" in process:
                            bin_range = "200p0_10000p0"
                        elif "GT450" in process:
                            bin_range = "450p0_10000p0"
                        else:
                            bin_range = [bnr for bnr in list(self.full_scaling_names[production_mode].keys()) if bnr in process][0]
                elif obs == "Njets":
                    if "xH" not in process:
                        if any(exp in process for exp in ["NJ_0", "NJ_0p0_1p0"]):
                            bin_range = "m0p5_0p5"
                        elif any(exp in process for exp in ["NJ_1", "NJ_1p0_2p0"]):
                            bin_range = "0p5_1p5"
                        elif any(exp in process for exp in ["NJ_2", "NJ_2p0_3p0"]):
                            bin_range = "1p5_2p5"
                        elif any(exp in process for exp in ["NJ_3", "NJ_3p0_4p0"]):
                            bin_range = "2p5_3p5"
                        elif any(exp in process for exp in ["NJ_G", "NJ_4p0_14p0"]):
                            bin_range = "3p5_100p5"
                    else:
                        bin_range = "not_supported"
                full_scaling_function = self.full_scaling_names[production_mode][bin_range]
                print("Scaling process {} in bin {} with function {}".format(process, bin, full_scaling_function))
                return full_scaling_function
            except (KeyError, IndexError) as e:
                print(e)
                print("WARNING: No scaling function for process {} in bin {}, will scale with 1".format(process, bin))
                return 1
        return 1


diff_to_smeft_model = DIFFtoSMEFTModel()
