//====================================================================
#include "AliBasedNdetaTask.h"
#include <TMath.h>
#include <TH2D.h>
#include <TH1D.h>
#include <THStack.h>
#include <TList.h>
#include <AliAnalysisManager.h>
#include <AliAODEvent.h>
#include <AliAODHandler.h>
#include <AliAODInputHandler.h>
#include "AliForwardUtil.h"
#include "AliAODForwardMult.h"

//____________________________________________________________________
AliBasedNdetaTask::AliBasedNdetaTask()
  : AliAnalysisTaskSE(), 
    fSum(0),	        //  Sum of histograms 
    fSumMC(0),	        //  Sum of MC histograms (if any)
    fSums(0),		// Container of sums 
    fOutput(0),		// Container of outputs 
    fTriggers(0),	// Histogram of triggers 
    fVtxMin(0),		// Minimum v_z
    fVtxMax(0),		// Maximum v_z
    fTriggerMask(0),    // Trigger mask 
    fRebin(0),       	// Rebinning factor 
    fCutEdges(false), 
    fSymmetrice(true),
    fCorrEmpty(true)
{}

//____________________________________________________________________
AliBasedNdetaTask::AliBasedNdetaTask(const char* name)
  : AliAnalysisTaskSE(name), 
    fSum(0),	        // Sum of histograms 
    fSumMC(0),	        // Sum of MC histograms (if any)
    fSums(0),	        // Container of sums 
    fOutput(0),		// Container of outputs 
    fTriggers(0),	// Histogram of triggers 
    fVtxMin(-10),	// Minimum v_z
    fVtxMax(10),	// Maximum v_z
    fTriggerMask(AliAODForwardMult::kInel), 
    fRebin(5),		// Rebinning factor 
    fCutEdges(false), 
    fSymmetrice(true),
    fCorrEmpty(true)
{
  // Output slot #1 writes into a TH1 container
  DefineOutput(1, TList::Class()); 
  DefineOutput(2, TList::Class()); 
}

//____________________________________________________________________
AliBasedNdetaTask::AliBasedNdetaTask(const AliBasedNdetaTask& o)
  : AliAnalysisTaskSE(o), 
    fSum(o.fSum),	// TH2D* -  Sum of histograms 
    fSumMC(o.fSumMC),// TH2D* -  Sum of MC histograms (if any)
    fSums(o.fSums),		// TList* - Container of sums 
    fOutput(o.fOutput),		// TList* - Container of outputs 
    fTriggers(o.fTriggers),	// TH1D* - Histogram of triggers 
    fVtxMin(o.fVtxMin),		// Double_t - Minimum v_z
    fVtxMax(o.fVtxMax),		// Double_t - Maximum v_z
    fTriggerMask(o.fTriggerMask),// Int_t - Trigger mask 
    fRebin(o.fRebin),		// Int_t - Rebinning factor 
    fCutEdges(o.fCutEdges),	// Bool_t - Whether to cut edges when rebinning
    fSymmetrice(true),
    fCorrEmpty(true)
{}

//____________________________________________________________________
AliBasedNdetaTask::~AliBasedNdetaTask()
{
  if (fSums) { 
    fSums->Delete();
    delete fSums;
    fSums = 0;
  }
  if (fOutputs) { 
    fOutputs->Delete();
    delete fOutputs;
    fOutputs = 0;
  }
}

//________________________________________________________________________
void 
AliBasedNdetaTask::SetTriggerMask(const char* mask)
{
  UShort_t    trgMask = 0;
  TString     trgs(mask);
  trgs.ToUpper();
  TObjString* trg;
  TIter       next(trgs.Tokenize(" ,|"));
  while ((trg = static_cast<TObjString*>(next()))) { 
    TString s(trg->GetString());
    if      (s.IsNull()) continue;
    if      (s.CompareTo("INEL")  == 0) trgMask = AliAODForwardMult::kInel;
    else if (s.CompareTo("INEL>0")== 0) trgMask = AliAODForwardMult::kInelGt0;
    else if (s.CompareTo("NSD")   == 0) trgMask = AliAODForwardMult::kNSD;
    else 
      Warning("SetTriggerMask", "Unknown trigger %s", s.Data());
  }
  if (trgMask == 0) trgMask = 1;
  SetTriggerMask(trgMask);
}

//________________________________________________________________________
void 
AliBasedNdetaTask::UserCreateOutputObjects()
{
  // Create histograms
  // Called once (on the worker node)

  fOutput = new TList;
  fOutput->SetName(Form("%s_result", GetName()));
  fOutput->SetOwner();

  fSums = new TList;
  fSums->SetName(Form("%s_sums", GetName()));
  fSums->SetOwner();


  fTriggers = new TH1D("triggers", "Number of triggers", 
		       kAccepted, 1, kAccepted);
  fTriggers->SetYTitle("# of events");
  fTriggers->GetXaxis()->SetBinLabel(kAll,         "All events");
  fTriggers->GetXaxis()->SetBinLabel(kB,           "w/B trigger");
  fTriggers->GetXaxis()->SetBinLabel(kA,           "w/A trigger");
  fTriggers->GetXaxis()->SetBinLabel(kC,           "w/C trigger");
  fTriggers->GetXaxis()->SetBinLabel(kE,           "w/E trigger");
  fTriggers->GetXaxis()->SetBinLabel(kMB,          "w/Collision trigger");
  fTriggers->GetXaxis()->SetBinLabel(kWithVertex,  "w/Vertex");
  fTriggers->GetXaxis()->SetBinLabel(kWithTrigger, "w/Selected trigger");
  fTriggers->GetXaxis()->SetBinLabel(kAccepted,    "Accepted by cut");
  fTriggers->GetXaxis()->SetNdivisions(kAccepted, false);
  fTriggers->SetFillColor(kRed+1);
  fTriggers->SetFillStyle(3001);
  fTriggers->SetStats(0);
  fSums->Add(fTriggers);

  // Check that we have an AOD input handler 
  AliAnalysisManager* am = AliAnalysisManager::GetAnalysisManager();
  AliAODInputHandler* ah = 
    dynamic_cast<AliAODInputHandler*>(am->GetInputEventHandler());
  if (!ah) AliFatal("No AOD input handler set in analysis manager");

  // Post data for ALL output slots >0 here, to get at least an empty histogram
  PostData(1, fSums); 
}

//____________________________________________________________________
TH2D*
AliBasedNdetaTask::CloneHist(TH2D* in, const char* name) 
{
  if (!in) return 0;
  TH2D* ret = static_cast<TH2D*>(in->Clone(name));
  ret->SetDirectory(0);
  ret->Sumw2();
  fSums->Add(ret);

  return ret;
}

//____________________________________________________________________
Bool_t
AliBasedNdetaTask::CheckEvent(AliAODEvent* aod)
{
  TObject*           oForward   = aod->FindListObject("Forward");
  if (!oForward) { 
    AliWarning("No forward object found");
    return false;
  }
  AliAODForwardMult* forward   = static_cast<AliAODForwardMult*>(oForward);
  
  // Count event 
  fTriggers->AddBinContent(kAll);
  if (forward->IsTriggerBits(AliAODForwardMult::kB)) 
    fTriggers->AddBinContent(kB);
  if (forward->IsTriggerBits(AliAODForwardMult::kA)) 
    fTriggers->AddBinContent(kA);
  if (forward->IsTriggerBits(AliAODForwardMult::kC)) 
    fTriggers->AddBinContent(kC);
  if (forward->IsTriggerBits(AliAODForwardMult::kE)) 
    fTriggers->AddBinContent(kE);
  if (forward->IsTriggerBits(AliAODForwardMult::kInel)) 
    fTriggers->AddBinContent(kMB);

  // Check if we have an event of interest. 
  if (!forward->IsTriggerBits(fTriggerMask)) return false;
  fTriggers->AddBinContent(kWithTrigger);

  // Check that we have a valid vertex
  if (!forward->HasIpZ()) return false;
  fTriggers->AddBinContent(kWithVertex);

  // Check that vertex is within cuts 
  if (!forward->InRange(fVtxMin, fVtxMax)) return false;
  fTriggers->AddBinContent(kAccepted);

  return true;
}
//____________________________________________________________________
void 
AliBasedNdetaTask::UserExec(Option_t *) 
{
  // Main loop
  AliAODEvent* aod = dynamic_cast<AliAODEvent*>(InputEvent());
  if (!aod) {
    AliError("Cannot get the AOD event");
    return;
  }  

  // Get the histogram(s) 
  TH2D* data   = GetHistogram(aod);
  TH2D* dataMC = GetHistogram(aod, true);

  // We should have a base object at least 
  if (!data) { 
    AliWarning("No data object found in AOD");
    return;
  }
  
  // Create our sum histograms 
  if (!fSum)             fSum   = CloneHist(data,  GetName());
  if (!fSumMC && dataMC) fSumMC = CloneHist(dataMC,Form("%sMC", GetName()));

  // Check the event 
  if (!CheckEvent(aod)) return;

  // Add contribution 
  fSum->Add((data));
  if (fSumMC) fSumMC->Add((dataMC));

  PostData(1, fSums);
}

//________________________________________________________________________
void 
AliBasedNdetaTask::SetHistogramAttributes(TH1D* h, Int_t colour, Int_t marker,
					  const char* title, const char* ytitle)
{
  h->SetTitle(title);
  h->SetMarkerColor(colour);
  h->SetMarkerStyle(marker);
  h->SetMarkerSize(1);
  h->SetFillStyle(0);
  h->SetYTitle(ytitle);
  h->GetXaxis()->SetTitleFont(132);
  h->GetXaxis()->SetLabelFont(132);
  h->GetXaxis()->SetNdivisions(10);
  h->GetYaxis()->SetTitleFont(132);
  h->GetYaxis()->SetLabelFont(132);
  h->GetYaxis()->SetNdivisions(10);
  h->GetYaxis()->SetDecimals();
  h->SetStats(0);
}

//________________________________________________________________________
TH1D*
AliBasedNdetaTask::ProjectX(const TH2D* h, 
			    const char* name,
			    Int_t firstbin, 
			    Int_t lastbin, 
			    bool  corr,
			    bool  error) const
{
  if (!h) return 0;
#if USE_ROOT_PROJECT
  return h->ProjectionX(name, firstbin, lastbin, (error ? "e" : ""));
#endif
  
  TAxis* xaxis = h->GetXaxis();
  TAxis* yaxis = h->GetYaxis();
  TH1D*  ret   = new TH1D(name, h->GetTitle(), xaxis->GetNbins(), 
			  xaxis->GetXmin(), xaxis->GetXmax());
  static_cast<const TAttLine*>(h)->Copy(*ret);
  static_cast<const TAttFill*>(h)->Copy(*ret);
  static_cast<const TAttMarker*>(h)->Copy(*ret);
  ret->GetXaxis()->ImportAttributes(xaxis);

  Int_t first = firstbin; 
  Int_t last  = lastbin;
  if (first < 0)                         first = 0;
  else if (first >= yaxis->GetNbins()+1) first = yaxis->GetNbins();
  if      (last  < 0)                    last  = yaxis->GetNbins();
  else if (last  >  yaxis->GetNbins()+1) last  = yaxis->GetNbins();
  if (last-first < 0) { 
    AliWarning(Form("Nothing to project [%d,%d]", first, last));
    return 0;
    
  }

  // Loop over X bins 
  // AliInfo(Form("Projecting bins [%d,%d] of %s", first, last, h->GetName()));
  Int_t ybins = (last-first+1);
  for (Int_t xbin = 0; xbin <= xaxis->GetNbins()+1; xbin++) { 
    Double_t content = 0;
    Double_t error2  = 0;
    Int_t    nbins   = 0;
    
    
    for (Int_t ybin = first; ybin <= last; ybin++) { 
      Double_t c1 = h->GetCellContent(xbin, ybin);
      Double_t e1 = h->GetCellError(xbin, ybin);

      // Ignore empty bins 
      if (c1 < 1e-12) continue;
      if (e1 < 1e-12) {
	if (error) continue; 
	e1 = 1;
      }

      content    += c1;
      error2     += e1*e1;
      nbins++;
    } // for (ybin)
    if(content > 0 && nbins > 0) {
      Double_t factor = (corr ? Double_t(ybins) / nbins : 1);
      if (error) { 
	// Calculate weighted average
	ret->SetBinContent(xbin, content * factor);
	ret->SetBinError(xbin, factor * TMath::Sqrt(error2));
      }
      else 
	ret->SetBinContent(xbin, factor * content);
    }
  } // for (xbin)
  
  return ret;
}
  
//________________________________________________________________________
void 
AliBasedNdetaTask::Terminate(Option_t *) 
{
  // Draw result to screen, or perform fitting, normalizations Called
  // once at the end of the query
        
  fSums = dynamic_cast<TList*> (GetOutputData(1));
  if(!fSums) {
    AliError("Could not retrieve TList fSums"); 
    return; 
  }
  
  if (!fOutput) { 
    fOutput = new TList;
    fOutput->SetName(Form("%s_result", GetName()));
    fOutput->SetOwner();
  }

  fSum     = static_cast<TH2D*>(fSums->FindObject(GetName()));
  fSumMC   = static_cast<TH2D*>(fSums->FindObject(Form("%sMC", GetName())));
  fTriggers       = static_cast<TH1D*>(fSums->FindObject("triggers"));

  if (!fTriggers) { 
    AliError("Couldn't find histogram 'triggers' in list");
    return;
  }
  if (!fSum) { 
    AliError("Couldn't find histogram 'base' in list");
    return;
  }

  Int_t nAll        = Int_t(fTriggers->GetBinContent(kAll));
  Int_t nB          = Int_t(fTriggers->GetBinContent(kB));
  Int_t nA          = Int_t(fTriggers->GetBinContent(kA));
  Int_t nC          = Int_t(fTriggers->GetBinContent(kC));
  Int_t nE          = Int_t(fTriggers->GetBinContent(kE));
  Int_t nMB         = Int_t(fTriggers->GetBinContent(kMB));
  Int_t nTriggered  = Int_t(fTriggers->GetBinContent(kWithTrigger));
  Int_t nWithVertex = Int_t(fTriggers->GetBinContent(kWithVertex));
  Int_t nAccepted   = Int_t(fTriggers->GetBinContent(kAccepted));
  Int_t nGood       = nB - nA - nC + 2 * nE;
  if (nTriggered <= 0) { 
    AliError("Number of triggered events <= 0");
    return;
  }
  if (nGood <= 0) { 
    AliWarning(Form("Number of good events=%d=%d-%d-%d+2*%d<=0",
		    nGood, nB, nA, nC, nE));
    nGood = nMB;
  }
  Double_t vtxEff   = Double_t(nMB) / nTriggered * Double_t(nAccepted) / nGood;
  Double_t vNorm    = Double_t(nAccepted) / nGood;
  AliInfo(Form("Total of %9d events\n"
	       "                   of these %9d are minimum bias\n"
	       "                   of these %9d has a %s trigger\n" 
	       "                   of these %9d has a vertex\n" 
	       "                   of these %9d were in [%+4.1f,%+4.1f]cm\n"
	       "                   Triggers by type:\n"
	       "                     B   = %9d\n"
	       "                     A|C = %9d (%9d+%-9d)\n"
	       "                     E   = %9d\n"
	       "                   Implies %9d good triggers\n"
	       "                   Vertex efficiency: %f (%f)",
	       nAll, nMB, nTriggered, 
	       AliAODForwardMult::GetTriggerString(fTriggerMask),
	       nWithVertex, nAccepted,
	       fVtxMin, fVtxMax, 
	       nB, nA+nC, nA, nC, nE, nGood, vtxEff, vNorm));
  
  const char* name = GetName();

  // Get acceptance normalisation from underflow bins 
  TH1D* norm   = ProjectX(fSum, Form("norm%s",name), 0, 1, fCorrEmpty, false);
  // Project onto eta axis - _ignoring_underflow_bins_!
  TH1D* dndeta = ProjectX(fSum, Form("dndeta%s",name), 1, fSum->GetNbinsY(),
			  fCorrEmpty);
  // Normalize to the acceptance 
  dndeta->Divide(norm);
  // Scale by the vertex efficiency 
  dndeta->Scale(vNorm, "width");
  norm->Scale(1. / nAccepted);

  SetHistogramAttributes(dndeta, kRed+1, 20, Form("ALICE %s", name));
  SetHistogramAttributes(norm, kRed+1,20,Form("ALICE %s normalisation", name)); 

  fOutput->Add(fTriggers->Clone());
  if (fSymmetrice)   fOutput->Add(Symmetrice(dndeta));
  fOutput->Add(dndeta);
  fOutput->Add(norm);
  fOutput->Add(Rebin(dndeta));
  if (fSymmetrice)   fOutput->Add(Symmetrice(Rebin(dndeta)));

  if (fSumMC) { 
    // Get acceptance normalisation from underflow bins 
    TH1D* normMC   = ProjectX(fSumMC,Form("norm%sMC", name), 0, 1, 
			      fCorrEmpty, false);
    // Project onto eta axis - _ignoring_underflow_bins_!
    TH1D* dndetaMC = ProjectX(fSumMC,Form("dndeta%sMC", name),1,
			      fSum->GetNbinsY(), fCorrEmpty);
    // Normalize to the acceptance 
    dndetaMC->Divide(normMC);
    // Scale by the vertex efficiency 
    dndetaMC->Scale(vNorm, "width");
    normMC->Scale(1. / nAccepted);

    SetHistogramAttributes(dndetaMC, kRed+3, 21, Form("ALICE %s (MC)",name));
    SetHistogramAttributes(normMC, kRed+3, 21, Form("ALICE %s (MC) "
						    "normalisation",name)); 

    fOutput->Add(dndetaMC);
    if (fSymmetrice)   fOutput->Add(Symmetrice(dndetaMC));    
    fOutput->Add(normMC);
    fOutput->Add(Rebin(dndetaMC));
    if (fSymmetrice)   fOutput->Add(Symmetrice(Rebin(dndetaMC)));
  }

  TNamed* trigString = 
    new TNamed("trigString", AliAODForwardMult::GetTriggerString(fTriggerMask));
  trigString->SetUniqueID(fTriggerMask);
  fOutput->Add(trigString);

  TAxis* vtxAxis = new TAxis(1,fVtxMin,fVtxMax);
  vtxAxis->SetName("vtxAxis");
  vtxAxis->SetTitle(Form("v_{z}#in[%+5.1f,%+5.1f]cm", fVtxMin,fVtxMax));
  fOutput->Add(vtxAxis);

  PostData(2, fOutput);
}

//________________________________________________________________________
TH1D*
AliBasedNdetaTask::Rebin(const TH1D* h) const
{
  if (fRebin <= 1) return 0;

  Int_t nBins = h->GetNbinsX();
  if(nBins % fRebin != 0) {
    Warning("Rebin", "Rebin factor %d is not a devisor of current number "
	    "of bins %d in the histogram %s", fRebin, nBins, h->GetName());
    return 0;
  }
    
  // Make a copy 
  TH1D* tmp = static_cast<TH1D*>(h->Clone(Form("%s_rebin%02d", 
					       h->GetName(), fRebin)));
  tmp->Rebin(fRebin);
  tmp->SetDirectory(0);

  // The new number of bins 
  Int_t nBinsNew = nBins / fRebin;
  for(Int_t i = 1;i<= nBinsNew; i++) {
    Double_t content = 0;
    Double_t sumw    = 0;
    Double_t wsum    = 0;
    Int_t    nbins   = 0;
    for(Int_t j = 1; j<=fRebin;j++) {
      Int_t    bin = (i-1)*fRebin + j;
      Double_t c   =  h->GetBinContent(bin);

      if (c <= 0) continue;

      if (fCutEdges) {
	if (h->GetBinContent(bin+1)<=0 || 
	    h->GetBinContent(bin-1)) {
	  Warning("Rebin", "removing bin %d=%f of %s (%d=%f,%d=%f)", 
		  bin, c, h->GetName(), 
		  bin+1, h->GetBinContent(bin+1), 
		  bin-1, h->GetBinContent(bin-1));
	  continue;
	}	
      }
      Double_t e =  h->GetBinError(bin);
      Double_t w =  1 / (e*e); // 1/c/c
      content    += c;
      sumw       += w;
      wsum       += w * c;
      nbins++;
    }
      
    if(content > 0 && nbins > 0) {
      tmp->SetBinContent(i, wsum / sumw);
      tmp->SetBinError(i,1./TMath::Sqrt(sumw));
    }
  }
  
  return tmp;
}

//__________________________________________________________________
/** 
 * Make an extension of @a h to make it symmetric about 0 
 * 
 * @param h Histogram to symmertrice 
 * 
 * @return Symmetric extension of @a h 
 */
TH1* 
AliBasedNdetaTask::Symmetrice(const TH1* h) const
{
  Int_t nBins = h->GetNbinsX();
  TH1* s = static_cast<TH1*>(h->Clone(Form("%s_mirror", h->GetName())));
  s->SetTitle(Form("%s (mirrored)", h->GetTitle()));
  s->Reset();
  s->SetBins(nBins, -h->GetXaxis()->GetXmax(), -h->GetXaxis()->GetXmin());
  s->SetMarkerColor(h->GetMarkerColor());
  s->SetMarkerSize(h->GetMarkerSize());
  s->SetMarkerStyle(h->GetMarkerStyle()+4);
  s->SetFillColor(h->GetFillColor());
  s->SetFillStyle(h->GetFillStyle());
  s->SetDirectory(0);

  // Find the first and last bin with data 
  Int_t first = nBins+1;
  Int_t last  = 0;
  for (Int_t i = 1; i <= nBins; i++) { 
    if (h->GetBinContent(i) <= 0) continue; 
    first = TMath::Min(first, i);
    last  = TMath::Max(last,  i);
  }
    
  Double_t xfirst = h->GetBinCenter(first-1);
  Int_t    f1     = h->GetXaxis()->FindBin(-xfirst);
  Int_t    l2     = s->GetXaxis()->FindBin(xfirst);
  for (Int_t i = f1, j=l2; i <= last; i++,j--) { 
    s->SetBinContent(j, h->GetBinContent(i));
    s->SetBinError(j, h->GetBinError(i));
  }
  // Fill in overlap bin 
  s->SetBinContent(l2+1, h->GetBinContent(first));
  s->SetBinError(l2+1, h->GetBinError(first));
  return s;
}
