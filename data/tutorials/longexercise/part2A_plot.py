"""
advanced task 2A in combine long exercise
"""
import ROOT

f = ROOT.TFile("workspace_part2_tasks.root")
w = f.w

pdf = w.pdf('shapeBkg_signal_region_ttbar_morph')
x = w.var("CMS_th1x")
nuis = w.var("CMS_eff_t_highpt")

frame = x.frame()

pdf.plotOn(frame)

for val, color in zip([-4, 4], [ROOT.kRed, ROOT.kGreen]):
    nuis.setVal(val)
    pdf.plotOn(frame, ROOT.RooFit.LineColor(color), ROOT.RooFit.Name(f"nuisance_{val}"))

c = ROOT.TCanvas("c", "Canvas", 800, 600)
frame.Draw()
c.SaveAs("part2A_plot.png")  # Optional: Save to file
c.Update()