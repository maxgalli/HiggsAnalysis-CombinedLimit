combineCards.py signal_region=datacard_part3.txt ttbar_cr=datacard_part3_ttbar_cr.txt DY_cr=datacard_part3_DY_cr.txt &> part3_combined.txt
text2workspace.py part3_combined.txt -m 200 -o workspace_part3.root
