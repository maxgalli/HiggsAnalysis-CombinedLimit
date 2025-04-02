import ROOT
import sys
import os

#ROOT.gSystem.Load('../../../../../../lib/slc7_amd64_gcc12/libHiggsAnalysisCombinedLimit.so')
ROOT.gSystem.Load('../../../build/libHiggsAnalysisCombinedLimit.so')

# Check if the user provided an input file
if len(sys.argv) != 2:
    print("Usage: python script.py <input_json_file>")
    sys.exit(1)

# Get the input file from command line
input_file = sys.argv[1]

# Ensure the file exists
if not os.path.isfile(input_file):
    print(f"Error: File '{input_file}' not found.")
    sys.exit(1)

# Extract the base name without extension
base_name = os.path.splitext(os.path.basename(input_file))[0]
output_file = f"{base_name}_imported.root"

ws = ROOT.RooWorkspace("w")
tool = ROOT.RooJSONFactoryWSTool(ws)
tool.importJSON(input_file)
ws.writeToFile(output_file)
