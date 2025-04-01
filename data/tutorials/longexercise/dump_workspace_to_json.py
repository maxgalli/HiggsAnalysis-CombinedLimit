import ROOT
import sys
import os
ROOT.RooFit.JSONIO.importExpressions()
print("importers")
ROOT.RooFit.JSONIO.printImporters()
print("exporters")
ROOT.RooFit.JSONIO.printExporters()
print("factory expressions")
ROOT.RooFit.JSONIO.printFactoryExpressions()
print("export keys")
#ROOT.RooFit.JSONIO.printExportKeys()

# Check if the user provided an input file
if len(sys.argv) != 2:
    print("Usage: python script.py <input_root_file>")
    sys.exit(1)

# Get the input file from command line
input_file = sys.argv[1]

# Ensure the file exists
if not os.path.isfile(input_file):
    print(f"Error: File '{input_file}' not found.")
    sys.exit(1)

# Extract the base name without extension
base_name = os.path.splitext(os.path.basename(input_file))[0]
output_file = f"{base_name}.json"

# Open the ROOT file
f = ROOT.TFile(input_file, "READ")
ws = f.Get("w")

if not ws:
    print("Error: Workspace 'ws' not found in the ROOT file.")
    sys.exit(1)

print(ws)

# Export to JSON
tool = ROOT.RooJSONFactoryWSTool(ws)
tool.exportJSON(output_file)

print(f"Exported JSON file: {output_file}")
