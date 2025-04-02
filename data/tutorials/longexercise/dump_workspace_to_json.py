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
import json

def format_json(file_path, indent=4):
    try:
        # Read the JSON file
        with open(file_path, "r") as file:
            data = json.load(file)  # Parse JSON

        # Write back with proper indentation
        with open(file_path, "w") as file:
            json.dump(data, file, indent=indent, ensure_ascii=False)

        print(f"Successfully formatted: {file_path}")

    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON - {e}")
    except Exception as e:
        print(f"Error: {e}")

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
format_json(output_file)

print(f"Exported JSON file: {output_file}")
