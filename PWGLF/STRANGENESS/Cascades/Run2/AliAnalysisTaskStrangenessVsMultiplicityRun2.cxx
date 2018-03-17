/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// This task is meant to provide a lightweight alternative to the regular
// V0 and cascade analysis tasks that were used in Run 1, which produced full-size
// TTree objects for V0 and cascade candidates. Instead of that, the output
// for this task has been structured as follows:
//
//  Output 1: TList object containing some standard analysis histograms
//            for event counting.
//
//  Output 2: TList object containing all registered output objects for
//            the V0 analysis in AliV0Result format. The output objects will
//            each hold a TH3F with analysis-relevant information and the
//            configuration that was used to get to that.
//
//  Output 3: TList object containing all registered output objects for
//            the cascade analysis in AliCascadeResult format. The output objects will
//            each hold a TH3F with analysis-relevant information and the
//            configuration that was used to get to that.
//
//  Output 4: (optional) TTree object holding event characteristics. At the
//            moment only related to a single centrality estimator (default:
//            V0M). No downscaling option exists.
//
//  Output 5: (optional) TTree object containing V0 candidates to allow for
//            Run 1 legacy code to function and to allow for full information
//            to be saved. To keep output under control, a downscaling factor
//            can be applied (default: 0.001, configurable)
//
//  Output 6: (optional) TTree object containing cascade candidates to allow for
//            Run 1 legacy code to function and to allow for full information
//            to be saved. To keep output under control, a downscaling factor
//            can be applied (default: 0.001, configurable)
//
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class TTree;
class TParticle;
class TVector3;

//class AliMCEventHandler;
//class AliMCEvent;
//class AliStack;

class AliESDVertex;
class AliAODVertex;
class AliESDv0;
class AliAODv0;

#include <numeric>

#include <Riostream.h>
#include "TList.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TFile.h"
#include "THnSparse.h"
#include "TVector3.h"
#include "TCanvas.h"
#include "TMath.h"
#include "TLegend.h"
#include "TRandom3.h"
#include "TLorentzVector.h"
#include "TDatabasePDG.h"
#include "TProfile.h"
//#include "AliLog.h"

#include "AliESDEvent.h"
#include "AliAODEvent.h"
#include "AliV0vertexer.h"
#include "AliCascadeVertexer.h"
#include "AliLightV0vertexer.h"
#include "AliLightCascadeVertexer.h"
#include "AliESDpid.h"
#include "AliESDtrack.h"
#include "AliESDtrackCuts.h"
#include "AliInputEventHandler.h"
#include "AliAnalysisManager.h"
#include "AliMCEventHandler.h"
#include "AliMCEvent.h"
#include "AliStack.h"
#include "AliCentrality.h"
#include "AliOADBContainer.h"
#include "AliOADBMultSelection.h"
#include "AliMultEstimator.h"
#include "AliMultVariable.h"
#include "AliMultInput.h"
#include "AliMultSelection.h"

#include "AliTriggerIR.h"
#include "AliAODForwardMult.h"
#include "AliForwardUtil.h"
#include "AliCFContainer.h"
#include "AliMultiplicity.h"
#include "AliAODMCParticle.h"
#include "AliESDcascade.h"
#include "AliAODcascade.h"
#include "AliESDUtils.h"
#include "AliGenEventHeader.h"
#include "AliAnalysisTaskSE.h"
#include "AliAnalysisUtils.h"
#include "AliEventCuts.h"
#include "AliV0Result.h"
#include "AliCascadeResult.h"
#include "AliAnalysisTaskStrangenessVsMultiplicityRun2.h"

using std::cout;
using std::endl;

ClassImp(AliAnalysisTaskStrangenessVsMultiplicityRun2)

AliAnalysisTaskStrangenessVsMultiplicityRun2::AliAnalysisTaskStrangenessVsMultiplicityRun2()
: AliAnalysisTaskSE(), fListHist(0), fListV0(0), fListCascade(0), fTreeEvent(0), fTreeV0(0), fTreeCascade(0), fPIDResponse(0), fESDtrackCuts(0), fESDtrackCutsITSsa2010(0), fESDtrackCutsGlobal2015(0), fUtils(0), fRand(0),

//---> Flags controlling Event Tree output
fkSaveEventTree    ( kTRUE ), //no downscaling in this tree so far

//---> Flags controlling V0 TTree output
fkSaveV0Tree       ( kTRUE ),
fkDownScaleV0      ( kTRUE  ),
fDownScaleFactorV0 ( 0.001  ),
fkPreselectDedx ( kFALSE ),
fkUseOnTheFlyV0Cascading( kFALSE ),
fkDebugWrongPIDForTracking ( kFALSE ),
fkDebugBump(kFALSE),
fkDebugOOBPileup(kFALSE),
fkDoExtraEvSels(kTRUE),

//---> Flags controlling Cascade TTree output
fkSaveCascadeTree       ( kTRUE  ),
fkDownScaleCascade      ( kTRUE  ),
fDownScaleFactorCascade ( 0.001  ),

//---> Flags controlling Vertexers
fkRunVertexers    ( kFALSE ),
fkUseLightVertexer ( kTRUE ),
fkDoV0Refit       ( kTRUE ),
fkExtraCleanup    ( kTRUE ),

//---> Flag controlling trigger selection
fTrigType(AliVEvent::kMB),

//---> Min pT to save candidate
fMinPtToSave( 0.55 ),
fMaxPtToSave( 100.00 ),

//---> Variables for fTreeEvent
fCentrality(0),
fMVPileupFlag(kFALSE),
fOOBPileupFlag(kFALSE),
fNTOFClusters(-1),
fNTOFMatches(-1),
fNTracksITSsa2010(-1),
fNTracksGlobal2015(-1),
fNTracksGlobal2015TriggerPP(-1),
fAmplitudeV0A(-1.),
fAmplitudeV0C(-1.),
fNHitsFMDA(-1.),
fNHitsFMDC(-1.),
fClosestNonEmptyBC(-1),

//---> Variables for fTreeV0
fTreeVariableChi2V0(0),
fTreeVariableDcaV0Daughters(0),
fTreeVariableDcaV0ToPrimVertex(0),
fTreeVariableDcaPosToPrimVertex(0),
fTreeVariableDcaNegToPrimVertex(0),
fTreeVariableV0CosineOfPointingAngle(0),
fTreeVariableV0Radius(0),
fTreeVariablePt(0),
fTreeVariableRapK0Short(0),
fTreeVariableRapLambda(0),
fTreeVariableInvMassK0s(0),
fTreeVariableInvMassLambda(0),
fTreeVariableInvMassAntiLambda(0),
fTreeVariableAlphaV0(0),
fTreeVariablePtArmV0(0),
fTreeVariableNegEta(0),
fTreeVariablePosEta(0),

fTreeVariableNSigmasPosProton(0),
fTreeVariableNSigmasPosPion(0),
fTreeVariableNSigmasNegProton(0),
fTreeVariableNSigmasNegPion(0),

fTreeVariableDistOverTotMom(0),
fTreeVariableLeastNbrCrossedRows(0),
fTreeVariableLeastRatioCrossedRowsOverFindable(0),
fTreeVariableMaxChi2PerCluster(0),
fTreeVariableMinTrackLength(0),

fTreeVariablePosPIDForTracking(-1),
fTreeVariableNegPIDForTracking(-1),
fTreeVariablePosdEdx(-1),
fTreeVariableNegdEdx(-1),
fTreeVariablePosInnerP(-1),
fTreeVariableNegInnerP(-1),
fTreeVariableNegTrackStatus(0),
fTreeVariablePosTrackStatus(0),
fTreeVariableNegDCAz(-1),
fTreeVariablePosDCAz(-1),

fTreeVariableNegTOFExpTDiff(99999),
fTreeVariablePosTOFExpTDiff(99999),
fTreeVariableNegTOFSignal(99999),
fTreeVariablePosTOFSignal(99999),
fTreeVariableAmplitudeV0A(-1.),
fTreeVariableAmplitudeV0C(-1.),
fTreeVariableNHitsFMDA(-1.),
fTreeVariableNHitsFMDC(-1.),
fTreeVariableClosestNonEmptyBC(-1),

fTreeVariableCentrality(0),
fTreeVariableMVPileupFlag(kFALSE),
fTreeVariableOOBPileupFlag(kFALSE),

//---> Variables for fTreeCascade
fTreeCascVarCharge(0),
fTreeCascVarMassAsXi(0),
fTreeCascVarMassAsOmega(0),
fTreeCascVarPt(0),
fTreeCascVarRapXi(0),
fTreeCascVarRapOmega(0),
fTreeCascVarNegEta(0),
fTreeCascVarPosEta(0),
fTreeCascVarBachEta(0),
fTreeCascVarDCACascDaughters(0),
fTreeCascVarDCABachToPrimVtx(0),
fTreeCascVarDCAV0Daughters(0),
fTreeCascVarDCAV0ToPrimVtx(0),
fTreeCascVarDCAPosToPrimVtx(0),
fTreeCascVarDCANegToPrimVtx(0),
fTreeCascVarCascCosPointingAngle(0),
fTreeCascVarCascRadius(0),
fTreeCascVarV0Mass(0),
fTreeCascVarV0MassLambda(0),
fTreeCascVarV0MassAntiLambda(0),
fTreeCascVarV0CosPointingAngle(0),
fTreeCascVarV0CosPointingAngleSpecial(0),
fTreeCascVarV0Radius(0),
fTreeCascVarDCABachToBaryon(0),
fTreeCascVarWrongCosPA(0),
fTreeCascVarLeastNbrClusters(0),
fTreeCascVarDistOverTotMom(0),
fTreeCascVarMaxChi2PerCluster(0),
fTreeCascVarMinTrackLength(0),

fTreeCascVarNegNSigmaPion(0),
fTreeCascVarNegNSigmaProton(0),
fTreeCascVarPosNSigmaPion(0),
fTreeCascVarPosNSigmaProton(0),
fTreeCascVarBachNSigmaPion(0),
fTreeCascVarBachNSigmaKaon(0),

fTreeCascVarNegTOFNSigmaPion(0),
fTreeCascVarNegTOFNSigmaProton(0),
fTreeCascVarPosTOFNSigmaPion(0),
fTreeCascVarPosTOFNSigmaProton(0),
fTreeCascVarBachTOFNSigmaPion(0),
fTreeCascVarBachTOFNSigmaKaon(0),

fTreeCascVarNegITSNSigmaPion(0),
fTreeCascVarNegITSNSigmaProton(0),
fTreeCascVarPosITSNSigmaPion(0),
fTreeCascVarPosITSNSigmaProton(0),
fTreeCascVarBachITSNSigmaPion(0),
fTreeCascVarBachITSNSigmaKaon(0),

fTreeCascVarChiSquareV0(1e+3),
fTreeCascVarChiSquareCascade(1e+3),

fTreeCascVarBachDCAPVSigmaX2(0),
fTreeCascVarBachDCAPVSigmaY2(0),
fTreeCascVarBachDCAPVSigmaZ2(0),
fTreeCascVarPosDCAPVSigmaX2(0),
fTreeCascVarPosDCAPVSigmaY2(0),
fTreeCascVarPosDCAPVSigmaZ2(0),
fTreeCascVarNegDCAPVSigmaX2(0),
fTreeCascVarNegDCAPVSigmaY2(0),
fTreeCascVarNegDCAPVSigmaZ2(0),

fTreeCascVarPosPIDForTracking(-1),
fTreeCascVarNegPIDForTracking(-1),
fTreeCascVarBachPIDForTracking(-1),
fTreeCascVarNegInnerP(-1),
fTreeCascVarPosInnerP(-1),
fTreeCascVarBachInnerP(-1),
fTreeCascVarNegdEdx(-1),
fTreeCascVarPosdEdx(-1),
fTreeCascVarBachdEdx(-1),

fTreeCascVarNegTrackStatus(0), //!
fTreeCascVarPosTrackStatus(0), //!
fTreeCascVarBachTrackStatus(0), //!
fTreeCascVarNegDCAz(-1),
fTreeCascVarPosDCAz(-1),
fTreeCascVarBachDCAz(-1),
//fTreeCascVarPosTotMom(-1),
//fTreeCascVarNegTotMom(-1),
//fTreeCascVarBachTotMom(-1),

//Variables for debugging the invariant mass bump
//Full momentum information
fTreeCascVarNegPx(0),
fTreeCascVarNegPy(0),
fTreeCascVarNegPz(0),
fTreeCascVarPosPx(0),
fTreeCascVarPosPy(0),
fTreeCascVarPosPz(0),
fTreeCascVarBachPx(0),
fTreeCascVarBachPy(0),
fTreeCascVarBachPz(0),
fTreeCascVarV0DecayX(0),
fTreeCascVarV0DecayY(0),
fTreeCascVarV0DecayZ(0),
fTreeCascVarCascadeDecayX(0),
fTreeCascVarCascadeDecayY(0),
fTreeCascVarCascadeDecayZ(0),
fTreeCascVarV0Lifetime(0),
//Track Labels (check for duplicates, etc)
fTreeCascVarNegIndex(0),
fTreeCascVarPosIndex(0),
fTreeCascVarBachIndex(0),
//Event Number (check same-event index mixups)
fTreeCascVarEventNumber(0),

fTreeCascVarNegTOFExpTDiff(99999),
fTreeCascVarPosTOFExpTDiff(99999),
fTreeCascVarBachTOFExpTDiff(99999),
fTreeCascVarNegTOFSignal(99999),
fTreeCascVarPosTOFSignal(99999),
fTreeCascVarBachTOFSignal(99999),
fTreeCascVarAmplitudeV0A(-1.),
fTreeCascVarAmplitudeV0C(-1.),
fTreeCascVarNHitsFMDA(-1.),
fTreeCascVarNHitsFMDC(-1.),
fTreeCascVarClosestNonEmptyBC(-1),

fTreeCascVarCentrality(0),
fTreeCascVarMVPileupFlag(kFALSE),
fTreeCascVarOOBPileupFlag(kFALSE),
//Kink tagging
fTreeCascVarBachIsKink(kFALSE),
fTreeCascVarPosIsKink(kFALSE),
fTreeCascVarNegIsKink(kFALSE),
fkSelectCharge(0),
//Histos
fHistEventCounter(0),
fHistCentrality(0)
//------------------------------------------------
// Tree Variables
{
    
}

AliAnalysisTaskStrangenessVsMultiplicityRun2::AliAnalysisTaskStrangenessVsMultiplicityRun2(Bool_t lSaveEventTree, Bool_t lSaveV0Tree, Bool_t lSaveCascadeTree, const char *name, TString lExtraOptions)
: AliAnalysisTaskSE(name), fListHist(0), fListV0(0), fListCascade(0), fTreeEvent(0), fTreeV0(0), fTreeCascade(0), fPIDResponse(0), fESDtrackCuts(0), fESDtrackCutsITSsa2010(0), fESDtrackCutsGlobal2015(0), fUtils(0), fRand(0),

//---> Flags controlling Event Tree output
fkSaveEventTree    ( kFALSE ), //no downscaling in this tree so far

//---> Flags controlling V0 TTree output
fkSaveV0Tree       ( kTRUE ),
fkDownScaleV0      ( kTRUE  ),
fDownScaleFactorV0 ( 0.001  ),
fkPreselectDedx ( kFALSE ),
fkUseOnTheFlyV0Cascading( kFALSE ),
fkDebugWrongPIDForTracking ( kFALSE ), //also for cascades...
fkDebugBump( kFALSE ),
fkDebugOOBPileup(kFALSE),
fkDoExtraEvSels(kTRUE),

//---> Flags controlling Cascade TTree output
fkSaveCascadeTree       ( kTRUE  ),
fkDownScaleCascade      ( kTRUE  ),
fDownScaleFactorCascade ( 0.001  ),

//---> Flags controlling Vertexers
fkRunVertexers    ( kFALSE ),
fkUseLightVertexer ( kTRUE ),
fkDoV0Refit       ( kTRUE ),
fkExtraCleanup    ( kTRUE ),

//---> Flag controlling trigger selection
fTrigType(AliVEvent::kMB),

//---> Min pT to save candidate
fMinPtToSave( 0.55 ),
fMaxPtToSave( 100.00 ),

//---> Variables for fTreeEvent
fCentrality(0),
fMVPileupFlag(kFALSE),
fOOBPileupFlag(kFALSE),
fNTOFClusters(-1),
fNTOFMatches(-1),
fNTracksITSsa2010(-1),
fNTracksGlobal2015(-1),
fNTracksGlobal2015TriggerPP(-1),
fAmplitudeV0A(-1.),
fAmplitudeV0C(-1.),
fNHitsFMDA(-1.),
fNHitsFMDC(-1.),
fClosestNonEmptyBC(-1),

//---> Variables for fTreeV0
fTreeVariableChi2V0(0),
fTreeVariableDcaV0Daughters(0),
fTreeVariableDcaV0ToPrimVertex(0),
fTreeVariableDcaPosToPrimVertex(0),
fTreeVariableDcaNegToPrimVertex(0),
fTreeVariableV0CosineOfPointingAngle(0),
fTreeVariableV0Radius(0),
fTreeVariablePt(0),
fTreeVariableRapK0Short(0),
fTreeVariableRapLambda(0),
fTreeVariableInvMassK0s(0),
fTreeVariableInvMassLambda(0),
fTreeVariableInvMassAntiLambda(0),
fTreeVariableAlphaV0(0),
fTreeVariablePtArmV0(0),
fTreeVariableNegEta(0),
fTreeVariablePosEta(0),

fTreeVariableNSigmasPosProton(0),
fTreeVariableNSigmasPosPion(0),
fTreeVariableNSigmasNegProton(0),
fTreeVariableNSigmasNegPion(0),

fTreeVariableDistOverTotMom(0),
fTreeVariableLeastNbrCrossedRows(0),
fTreeVariableLeastRatioCrossedRowsOverFindable(0),
fTreeVariableMaxChi2PerCluster(0),
fTreeVariableMinTrackLength(0),

fTreeVariablePosPIDForTracking(-1),
fTreeVariableNegPIDForTracking(-1),
fTreeVariablePosdEdx(-1),
fTreeVariableNegdEdx(-1),
fTreeVariablePosInnerP(-1),
fTreeVariableNegInnerP(-1),
fTreeVariableNegTrackStatus(0),
fTreeVariablePosTrackStatus(0),
fTreeVariableNegDCAz(-1),
fTreeVariablePosDCAz(-1),

fTreeVariableNegTOFExpTDiff(99999),
fTreeVariablePosTOFExpTDiff(99999),
fTreeVariableNegTOFSignal(99999),
fTreeVariablePosTOFSignal(99999),
fTreeVariableAmplitudeV0A(-1.),
fTreeVariableAmplitudeV0C(-1.),
fTreeVariableNHitsFMDA(-1.),
fTreeVariableNHitsFMDC(-1.),
fTreeVariableClosestNonEmptyBC(-1),

fTreeVariableCentrality(0),
fTreeVariableMVPileupFlag(kFALSE),
fTreeVariableOOBPileupFlag(kFALSE),

//---> Variables for fTreeCascade
fTreeCascVarCharge(0),
fTreeCascVarMassAsXi(0),
fTreeCascVarMassAsOmega(0),
fTreeCascVarPt(0),
fTreeCascVarRapXi(0),
fTreeCascVarRapOmega(0),
fTreeCascVarNegEta(0),
fTreeCascVarPosEta(0),
fTreeCascVarBachEta(0),
fTreeCascVarDCACascDaughters(0),
fTreeCascVarDCABachToPrimVtx(0),
fTreeCascVarDCAV0Daughters(0),
fTreeCascVarDCAV0ToPrimVtx(0),
fTreeCascVarDCAPosToPrimVtx(0),
fTreeCascVarDCANegToPrimVtx(0),
fTreeCascVarCascCosPointingAngle(0),
fTreeCascVarCascRadius(0),
fTreeCascVarV0Mass(0),
fTreeCascVarV0MassLambda(0),
fTreeCascVarV0MassAntiLambda(0),
fTreeCascVarV0CosPointingAngle(0),
fTreeCascVarV0CosPointingAngleSpecial(0),
fTreeCascVarV0Radius(0),
fTreeCascVarDCABachToBaryon(0),
fTreeCascVarWrongCosPA(0),
fTreeCascVarLeastNbrClusters(0),
fTreeCascVarDistOverTotMom(0),
fTreeCascVarMaxChi2PerCluster(0),
fTreeCascVarMinTrackLength(0),

fTreeCascVarNegNSigmaPion(0),
fTreeCascVarNegNSigmaProton(0),
fTreeCascVarPosNSigmaPion(0),
fTreeCascVarPosNSigmaProton(0),
fTreeCascVarBachNSigmaPion(0),
fTreeCascVarBachNSigmaKaon(0),

fTreeCascVarNegITSNSigmaPion(0),
fTreeCascVarNegITSNSigmaProton(0),
fTreeCascVarPosITSNSigmaPion(0),
fTreeCascVarPosITSNSigmaProton(0),
fTreeCascVarBachITSNSigmaPion(0),
fTreeCascVarBachITSNSigmaKaon(0),

fTreeCascVarNegTOFNSigmaPion(0),
fTreeCascVarNegTOFNSigmaProton(0),
fTreeCascVarPosTOFNSigmaPion(0),
fTreeCascVarPosTOFNSigmaProton(0),
fTreeCascVarBachTOFNSigmaPion(0),
fTreeCascVarBachTOFNSigmaKaon(0),

fTreeCascVarChiSquareV0(1e+3),
fTreeCascVarChiSquareCascade(1e+3),

fTreeCascVarBachDCAPVSigmaX2(0),
fTreeCascVarBachDCAPVSigmaY2(0),
fTreeCascVarBachDCAPVSigmaZ2(0),
fTreeCascVarPosDCAPVSigmaX2(0),
fTreeCascVarPosDCAPVSigmaY2(0),
fTreeCascVarPosDCAPVSigmaZ2(0),
fTreeCascVarNegDCAPVSigmaX2(0),
fTreeCascVarNegDCAPVSigmaY2(0),
fTreeCascVarNegDCAPVSigmaZ2(0),

fTreeCascVarPosPIDForTracking(-1),
fTreeCascVarNegPIDForTracking(-1),
fTreeCascVarBachPIDForTracking(-1),
fTreeCascVarNegInnerP(-1),
fTreeCascVarPosInnerP(-1),
fTreeCascVarBachInnerP(-1),
fTreeCascVarNegdEdx(-1),
fTreeCascVarPosdEdx(-1),
fTreeCascVarBachdEdx(-1),

fTreeCascVarNegTrackStatus(0), //!
fTreeCascVarPosTrackStatus(0), //!
fTreeCascVarBachTrackStatus(0), //!
fTreeCascVarNegDCAz(-1),
fTreeCascVarPosDCAz(-1),
fTreeCascVarBachDCAz(-1),
//fTreeCascVarPosTotMom(-1),
//fTreeCascVarNegTotMom(-1),
//fTreeCascVarBachTotMom(-1),


//Variables for debugging the invariant mass bump
//Full momentum information
fTreeCascVarNegPx(0),
fTreeCascVarNegPy(0),
fTreeCascVarNegPz(0),
fTreeCascVarPosPx(0),
fTreeCascVarPosPy(0),
fTreeCascVarPosPz(0),
fTreeCascVarBachPx(0),
fTreeCascVarBachPy(0),
fTreeCascVarBachPz(0),
fTreeCascVarV0DecayX(0),
fTreeCascVarV0DecayY(0),
fTreeCascVarV0DecayZ(0),
fTreeCascVarCascadeDecayX(0),
fTreeCascVarCascadeDecayY(0),
fTreeCascVarCascadeDecayZ(0),
fTreeCascVarV0Lifetime(0),
//Track Labels (check for duplicates, etc)
fTreeCascVarNegIndex(0),
fTreeCascVarPosIndex(0),
fTreeCascVarBachIndex(0),
//Event Number (check same-event index mixups)
fTreeCascVarEventNumber(0),

fTreeCascVarNegTOFExpTDiff(99999),
fTreeCascVarPosTOFExpTDiff(99999),
fTreeCascVarBachTOFExpTDiff(99999),
fTreeCascVarNegTOFSignal(99999),
fTreeCascVarPosTOFSignal(99999),
fTreeCascVarBachTOFSignal(99999),
fTreeCascVarAmplitudeV0A(-1.),
fTreeCascVarAmplitudeV0C(-1.),
fTreeCascVarNHitsFMDA(-1.),
fTreeCascVarNHitsFMDC(-1.),
fTreeCascVarClosestNonEmptyBC(-1),

fTreeCascVarCentrality(0),
fTreeCascVarMVPileupFlag(kFALSE),
fTreeCascVarOOBPileupFlag(kFALSE),
//Kink tagging
fTreeCascVarBachIsKink(kFALSE),
fTreeCascVarPosIsKink(kFALSE),
fTreeCascVarNegIsKink(kFALSE),
fkSelectCharge(0),
//Histos
fHistEventCounter(0),
fHistCentrality(0)
{
    
    //Re-vertex: Will only apply for cascade candidates
    
    fV0VertexerSels[0] =  33.  ;  // max allowed chi2
    fV0VertexerSels[1] =   0.02;  // min allowed impact parameter for the 1st daughter (LHC09a4 : 0.05)
    fV0VertexerSels[2] =   0.02;  // min allowed impact parameter for the 2nd daughter (LHC09a4 : 0.05)
    fV0VertexerSels[3] =   2.0 ;  // max allowed DCA between the daughter tracks       (LHC09a4 : 0.5)
    fV0VertexerSels[4] =   0.95;  // min allowed cosine of V0's pointing angle         (LHC09a4 : 0.99)
    fV0VertexerSels[5] =   1.0 ;  // min radius of the fiducial volume                 (LHC09a4 : 0.2)
    fV0VertexerSels[6] = 200.  ;  // max radius of the fiducial volume                 (LHC09a4 : 100.0)
    
    fCascadeVertexerSels[0] =  33.   ;  // max allowed chi2 (same as PDC07)
    fCascadeVertexerSels[1] =   0.05 ;  // min allowed V0 impact parameter                    (PDC07 : 0.05   / LHC09a4 : 0.025 )
    fCascadeVertexerSels[2] =   0.010;  // "window" around the Lambda mass                    (PDC07 : 0.008  / LHC09a4 : 0.010 )
    fCascadeVertexerSels[3] =   0.03 ;  // min allowed bachelor's impact parameter            (PDC07 : 0.035  / LHC09a4 : 0.025 )
    fCascadeVertexerSels[4] =   2.0  ;  // max allowed DCA between the V0 and the bachelor    (PDC07 : 0.1    / LHC09a4 : 0.2   )
    fCascadeVertexerSels[5] =   0.95 ;  // min allowed cosine of the cascade pointing angle   (PDC07 : 0.9985 / LHC09a4 : 0.998 )
    fCascadeVertexerSels[6] =   0.4  ;  // min radius of the fiducial volume                  (PDC07 : 0.9    / LHC09a4 : 0.2   )
    fCascadeVertexerSels[7] = 100.   ;  // max radius of the fiducial volume                  (PDC07 : 100    / LHC09a4 : 100   )
    
    //[0]+[1]*TMath::Exp([2]*x)+[3]*TMath::Exp([4]*x)
    fLambdaMassMean[0]=1.116; //standard fixed
    fLambdaMassMean[1]=0.0;
    fLambdaMassMean[2]=0.0;
    fLambdaMassMean[3]=0.0;
    fLambdaMassMean[4]=0.0;
    
    //[0]+[1]*x+[2]*TMath::Exp([3]*x)
    fLambdaMassSigma[0]=0.002; //standard at roughly the integ val
    fLambdaMassSigma[1]=0.0;
    fLambdaMassSigma[2]=0.0;
    fLambdaMassSigma[3]=0.0;
    
    fkSaveEventTree    = lSaveEventTree;
    fkSaveV0Tree       = lSaveV0Tree;
    fkSaveCascadeTree  = lSaveCascadeTree;
    
    //Standard output
    DefineOutput(1, TList::Class()); // Basic Histograms
    DefineOutput(2, TList::Class()); // V0 Histogram Output
    DefineOutput(3, TList::Class()); // Cascade Histogram Output
    
    //Optional output
    if (fkSaveEventTree)
    DefineOutput(4, TTree::Class()); // Event Tree output
    if (fkSaveV0Tree)
    DefineOutput(5, TTree::Class()); // V0 Tree output
    if (fkSaveCascadeTree)
    DefineOutput(6, TTree::Class()); // Cascade Tree output
    
    //Special Debug Options (more to be added as needed)
    // A - Study Wrong PID for tracking bug
    // B - Study invariant mass *B*ump
    // C - Study OOB pileup in pp 2016 data
    if ( lExtraOptions.Contains("A") ) fkDebugWrongPIDForTracking = kTRUE;
    if ( lExtraOptions.Contains("B") ) fkDebugBump                = kTRUE;
    if ( lExtraOptions.Contains("C") ) fkDebugOOBPileup           = kTRUE;
    
}


AliAnalysisTaskStrangenessVsMultiplicityRun2::~AliAnalysisTaskStrangenessVsMultiplicityRun2()
{
    //------------------------------------------------
    // DESTRUCTOR
    //------------------------------------------------
    
    //Destroy output objects if present
    if (fListHist) {
        delete fListHist;
        fListHist = 0x0;
    }
    if (fListV0) {
        delete fListV0;
        fListV0 = 0x0;
    }
    if (fListCascade) {
        delete fListCascade;
        fListCascade = 0x0;
    }
    if (fTreeEvent) {
        delete fTreeEvent;
        fTreeEvent = 0x0;
    }
    if (fTreeV0) {
        delete fTreeV0;
        fTreeV0 = 0x0;
    }
    if (fTreeCascade) {
        delete fTreeCascade;
        fTreeCascade = 0x0;
    }
    if (fUtils) {
        delete fUtils;
        fUtils = 0x0;
    }
    if (fRand) {
        delete fRand;
        fRand = 0x0;
    }
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::UserCreateOutputObjects()
{
    //------------------------------------------------
    // fTreeEvent: EbyE information
    //------------------------------------------------
    if(fkSaveEventTree){
        fTreeEvent = new TTree("fTreeEvent","Event");
        //Branch Definitions
        fTreeEvent->Branch("fCentrality",&fCentrality,"fCentrality/F");
        fTreeEvent->Branch("fMVPileupFlag",&fMVPileupFlag,"fMVPileupFlag/O");
        //
        if ( fkDebugOOBPileup ){
            fTreeEvent->Branch("fOOBPileupFlag",&fOOBPileupFlag,"fOOBPileupFlag/O");
            fTreeEvent->Branch("fNTOFClusters",&fNTOFClusters,"fNTOFClusters/I");
            fTreeEvent->Branch("fNTOFMatches",&fNTOFMatches,"fNTOFMatches/I");
            fTreeEvent->Branch("fNTracksITSsa2010",&fNTracksITSsa2010,"fNTracksITSsa2010/I");
            fTreeEvent->Branch("fNTracksGlobal2015",&fNTracksGlobal2015,"fNTracksGlobal2015/I");
            fTreeEvent->Branch("fNTracksGlobal2015TriggerPP",&fNTracksGlobal2015TriggerPP,"fNTracksGlobal2015TriggerPP/I");
            fTreeEvent->Branch("fAmplitudeV0A",&fAmplitudeV0A,"fAmplitudeV0A/F");
            fTreeEvent->Branch("fAmplitudeV0C",&fAmplitudeV0C,"fAmplitudeV0C/F");
            fTreeEvent->Branch("fNHitsFMDA",&fNHitsFMDA,"fNHitsFMDA/F");
            fTreeEvent->Branch("fNHitsFMDC",&fNHitsFMDC,"fNHitsFMDC/F");
            fTreeEvent->Branch("fClosestNonEmptyBC",&fClosestNonEmptyBC,"fClosestNonEmptyBC/I");
        }
    }
    
    //------------------------------------------------
    // fTreeV0: V0 Candidate Information
    //------------------------------------------------
    if(fkSaveV0Tree){
        //Create Basic V0 Output Tree
        fTreeV0 = new TTree( "fTreeV0", "V0 Candidates");
        //-----------BASIC-INFO---------------------------
        fTreeV0->Branch("fTreeVariableChi2V0",&fTreeVariableChi2V0,"fTreeVariableChi2V0/F");
        fTreeV0->Branch("fTreeVariableDcaV0Daughters",&fTreeVariableDcaV0Daughters,"fTreeVariableDcaV0Daughters/F");
        fTreeV0->Branch("fTreeVariableDcaV0ToPrimVertex",&fTreeVariableDcaV0ToPrimVertex,"fTreeVariableDcaV0ToPrimVertex/F");
        fTreeV0->Branch("fTreeVariableDcaPosToPrimVertex",&fTreeVariableDcaPosToPrimVertex,"fTreeVariableDcaPosToPrimVertex/F");
        fTreeV0->Branch("fTreeVariableDcaNegToPrimVertex",&fTreeVariableDcaNegToPrimVertex,"fTreeVariableDcaNegToPrimVertex/F");
        fTreeV0->Branch("fTreeVariableV0Radius",&fTreeVariableV0Radius,"fTreeVariableV0Radius/F");
        fTreeV0->Branch("fTreeVariablePt",&fTreeVariablePt,"fTreeVariablePt/F");
        fTreeV0->Branch("fTreeVariableRapK0Short",&fTreeVariableRapK0Short,"fTreeVariableRapK0Short/F");
        fTreeV0->Branch("fTreeVariableRapLambda",&fTreeVariableRapLambda,"fTreeVariableRapLambda/F");
        fTreeV0->Branch("fTreeVariableInvMassK0s",&fTreeVariableInvMassK0s,"fTreeVariableInvMassK0s/F");
        fTreeV0->Branch("fTreeVariableInvMassLambda",&fTreeVariableInvMassLambda,"fTreeVariableInvMassLambda/F");
        fTreeV0->Branch("fTreeVariableInvMassAntiLambda",&fTreeVariableInvMassAntiLambda,"fTreeVariableInvMassAntiLambda/F");
        fTreeV0->Branch("fTreeVariableV0CosineOfPointingAngle",&fTreeVariableV0CosineOfPointingAngle,"fTreeVariableV0CosineOfPointingAngle/F");
        fTreeV0->Branch("fTreeVariableAlphaV0",&fTreeVariableAlphaV0,"fTreeVariableAlphaV0/F");
        fTreeV0->Branch("fTreeVariablePtArmV0",&fTreeVariablePtArmV0,"fTreeVariablePtArmV0/F");
        fTreeV0->Branch("fTreeVariableLeastNbrCrossedRows",&fTreeVariableLeastNbrCrossedRows,"fTreeVariableLeastNbrCrossedRows/I");
        fTreeV0->Branch("fTreeVariableLeastRatioCrossedRowsOverFindable",&fTreeVariableLeastRatioCrossedRowsOverFindable,"fTreeVariableLeastRatioCrossedRowsOverFindable/F");
        fTreeV0->Branch("fTreeVariableMaxChi2PerCluster",&fTreeVariableMaxChi2PerCluster,"fTreeVariableMaxChi2PerCluster/F");
        fTreeV0->Branch("fTreeVariableMinTrackLength",&fTreeVariableMinTrackLength,"fTreeVariableMinTrackLength/F");
        fTreeV0->Branch("fTreeVariableDistOverTotMom",&fTreeVariableDistOverTotMom,"fTreeVariableDistOverTotMom/F");
        fTreeV0->Branch("fTreeVariableNSigmasPosProton",&fTreeVariableNSigmasPosProton,"fTreeVariableNSigmasPosProton/F");
        fTreeV0->Branch("fTreeVariableNSigmasPosPion",&fTreeVariableNSigmasPosPion,"fTreeVariableNSigmasPosPion/F");
        fTreeV0->Branch("fTreeVariableNSigmasNegProton",&fTreeVariableNSigmasNegProton,"fTreeVariableNSigmasNegProton/F");
        fTreeV0->Branch("fTreeVariableNSigmasNegPion",&fTreeVariableNSigmasNegPion,"fTreeVariableNSigmasNegPion/F");
        fTreeV0->Branch("fTreeVariableNegEta",&fTreeVariableNegEta,"fTreeVariableNegEta/F");
        fTreeV0->Branch("fTreeVariablePosEta",&fTreeVariablePosEta,"fTreeVariablePosEta/F");
        //-----------MULTIPLICITY-INFO--------------------
        fTreeV0->Branch("fTreeVariableCentrality",&fTreeVariableCentrality,"fTreeVariableCentrality/F");
        fTreeV0->Branch("fTreeVariableMVPileupFlag",&fTreeVariableMVPileupFlag,"fTreeVariableMVPileupFlag/O");
        //------------------------------------------------
        if ( fkDebugWrongPIDForTracking ){
            fTreeV0->Branch("fTreeVariablePosPIDForTracking",&fTreeVariablePosPIDForTracking,"fTreeVariablePosPIDForTracking/I");
            fTreeV0->Branch("fTreeVariableNegPIDForTracking",&fTreeVariableNegPIDForTracking,"fTreeVariableNegPIDForTracking/I");
            fTreeV0->Branch("fTreeVariablePosdEdx",&fTreeVariablePosdEdx,"fTreeVariablePosdEdx/F");
            fTreeV0->Branch("fTreeVariableNegdEdx",&fTreeVariableNegdEdx,"fTreeVariableNegdEdx/F");
            fTreeV0->Branch("fTreeVariablePosInnerP",&fTreeVariablePosInnerP,"fTreeVariablePosInnerP/F");
            fTreeV0->Branch("fTreeVariableNegInnerP",&fTreeVariableNegInnerP,"fTreeVariableNegInnerP/F");
            fTreeV0->Branch("fTreeVariableNegTrackStatus",&fTreeVariableNegTrackStatus,"fTreeVariableNegTrackStatus/l");
            fTreeV0->Branch("fTreeVariablePosTrackStatus",&fTreeVariablePosTrackStatus,"fTreeVariablePosTrackStatus/l");
            fTreeV0->Branch("fTreeVariableNegDCAz",&fTreeVariableNegDCAz,"fTreeVariableNegDCAz/F");
            fTreeV0->Branch("fTreeVariablePosDCAz",&fTreeVariablePosDCAz,"fTreeVariablePosDCAz/F");
        }
        if ( fkDebugOOBPileup ) {
            fTreeV0->Branch("fTreeVariableNegTOFExpTDiff",&fTreeVariableNegTOFExpTDiff,"fTreeVariableNegTOFExpTDiff/F");
            fTreeV0->Branch("fTreeVariablePosTOFExpTDiff",&fTreeVariablePosTOFExpTDiff,"fTreeVariablePosTOFExpTDiff/F");
            fTreeV0->Branch("fTreeVariableNegTOFSignal",&fTreeVariableNegTOFSignal,"fTreeVariableNegTOFSignal/F");
            fTreeV0->Branch("fTreeVariablePosTOFSignal",&fTreeVariablePosTOFSignal,"fTreeVariablePosTOFSignal/F");
            // Event info
            fTreeV0->Branch("fTreeVariableOOBPileupFlag",&fTreeVariableOOBPileupFlag,"fTreeVariableOOBPileupFlag/O");
            fTreeV0->Branch("fTreeVariableAmplitudeV0A",&fTreeVariableAmplitudeV0A,"fTreeVariableAmplitudeV0A/F");
            fTreeV0->Branch("fTreeVariableAmplitudeV0C",&fTreeVariableAmplitudeV0C,"fTreeVariableAmplitudeV0C/F");
            fTreeV0->Branch("fTreeVariableNHitsFMDA",&fTreeVariableNHitsFMDA,"fTreeVariableNHitsFMDA/F");
            fTreeV0->Branch("fTreeVariableNHitsFMDC",&fTreeVariableNHitsFMDC,"fTreeVariableNHitsFMDC/F");
            fTreeV0->Branch("fTreeVariableClosestNonEmptyBC",&fTreeVariableClosestNonEmptyBC,"fTreeVariableClosestNonEmptyBC/I");
        }
        //------------------------------------------------
    }
    
    //------------------------------------------------
    // fTreeCascade Branch definitions - Cascade Tree
    //------------------------------------------------
    if(fkSaveCascadeTree){
        //Create Cascade output tree
        fTreeCascade = new TTree("fTreeCascade","CascadeCandidates");
        //-----------BASIC-INFO---------------------------
        fTreeCascade->Branch("fTreeCascVarCharge",&fTreeCascVarCharge,"fTreeCascVarCharge/I");
        fTreeCascade->Branch("fTreeCascVarMassAsXi",&fTreeCascVarMassAsXi,"fTreeCascVarMassAsXi/F");
        fTreeCascade->Branch("fTreeCascVarMassAsOmega",&fTreeCascVarMassAsOmega,"fTreeCascVarMassAsOmega/F");
        fTreeCascade->Branch("fTreeCascVarPt",&fTreeCascVarPt,"fTreeCascVarPt/F");
        fTreeCascade->Branch("fTreeCascVarRapXi",&fTreeCascVarRapXi,"fTreeCascVarRapXi/F");
        fTreeCascade->Branch("fTreeCascVarRapOmega",&fTreeCascVarRapOmega,"fTreeCascVarRapOmega/F");
        fTreeCascade->Branch("fTreeCascVarNegEta",&fTreeCascVarNegEta,"fTreeCascVarNegEta/F");
        fTreeCascade->Branch("fTreeCascVarPosEta",&fTreeCascVarPosEta,"fTreeCascVarPosEta/F");
        fTreeCascade->Branch("fTreeCascVarBachEta",&fTreeCascVarBachEta,"fTreeCascVarBachEta/F");
        //-----------INFO-FOR-CUTS------------------------
        fTreeCascade->Branch("fTreeCascVarDCACascDaughters",&fTreeCascVarDCACascDaughters,"fTreeCascVarDCACascDaughters/F");
        fTreeCascade->Branch("fTreeCascVarDCABachToPrimVtx",&fTreeCascVarDCABachToPrimVtx,"fTreeCascVarDCABachToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarDCAV0Daughters",&fTreeCascVarDCAV0Daughters,"fTreeCascVarDCAV0Daughters/F");
        fTreeCascade->Branch("fTreeCascVarDCAV0ToPrimVtx",&fTreeCascVarDCAV0ToPrimVtx,"fTreeCascVarDCAV0ToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarDCAPosToPrimVtx",&fTreeCascVarDCAPosToPrimVtx,"fTreeCascVarDCAPosToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarDCANegToPrimVtx",&fTreeCascVarDCANegToPrimVtx,"fTreeCascVarDCANegToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarCascCosPointingAngle",&fTreeCascVarCascCosPointingAngle,"fTreeCascVarCascCosPointingAngle/F");
        fTreeCascade->Branch("fTreeCascVarCascDCAtoPVxy",&fTreeCascVarCascDCAtoPVxy,"fTreeCascVarCascDCAtoPVxy/F");
        fTreeCascade->Branch("fTreeCascVarCascDCAtoPVz",&fTreeCascVarCascDCAtoPVz,"fTreeCascVarCascDCAtoPVz/F");
        
        fTreeCascade->Branch("fTreeCascVarCascRadius",&fTreeCascVarCascRadius,"fTreeCascVarCascRadius/F");
        fTreeCascade->Branch("fTreeCascVarV0Mass",&fTreeCascVarV0Mass,"fTreeCascVarV0Mass/F");
        fTreeCascade->Branch("fTreeCascVarV0MassLambda",&fTreeCascVarV0MassLambda,"fTreeCascVarV0MassLambda/F");
        fTreeCascade->Branch("fTreeCascVarV0MassAntiLambda",&fTreeCascVarV0MassAntiLambda,"fTreeCascVarV0MassAntiLambda/F");
        fTreeCascade->Branch("fTreeCascVarV0CosPointingAngle",&fTreeCascVarV0CosPointingAngle,"fTreeCascVarV0CosPointingAngle/F");
        fTreeCascade->Branch("fTreeCascVarV0CosPointingAngleSpecial",&fTreeCascVarV0CosPointingAngleSpecial,"fTreeCascVarV0CosPointingAngleSpecial/F");
        fTreeCascade->Branch("fTreeCascVarV0Radius",&fTreeCascVarV0Radius,"fTreeCascVarV0Radius/F");
        fTreeCascade->Branch("fTreeCascVarDCABachToBaryon",&fTreeCascVarDCABachToBaryon,"fTreeCascVarDCABachToBaryon/F");
        fTreeCascade->Branch("fTreeCascVarWrongCosPA",&fTreeCascVarWrongCosPA,"fTreeCascVarWrongCosPA/F");
        fTreeCascade->Branch("fTreeCascVarLeastNbrClusters",&fTreeCascVarLeastNbrClusters,"fTreeCascVarLeastNbrClusters/I");
        fTreeCascade->Branch("fTreeCascVarMaxChi2PerCluster",&fTreeCascVarMaxChi2PerCluster,"fTreeCascVarMaxChi2PerCluster/F");
        fTreeCascade->Branch("fTreeCascVarMinTrackLength",&fTreeCascVarMinTrackLength,"fTreeCascVarMinTrackLength/F");
        //-----------MULTIPLICITY-INFO--------------------
        fTreeCascade->Branch("fTreeCascVarCentrality",&fTreeCascVarCentrality,"fTreeCascVarCentrality/F");
        fTreeCascade->Branch("fTreeCascVarMVPileupFlag",&fTreeCascVarMVPileupFlag,"fTreeCascVarMVPileupFlag/O");
        //-----------DECAY-LENGTH-INFO--------------------
        fTreeCascade->Branch("fTreeCascVarDistOverTotMom",&fTreeCascVarDistOverTotMom,"fTreeCascVarDistOverTotMom/F");
        //------------------------------------------------
        fTreeCascade->Branch("fTreeCascVarNegNSigmaPion",&fTreeCascVarNegNSigmaPion,"fTreeCascVarNegNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarNegNSigmaProton",&fTreeCascVarNegNSigmaProton,"fTreeCascVarNegNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarPosNSigmaPion",&fTreeCascVarPosNSigmaPion,"fTreeCascVarPosNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarPosNSigmaProton",&fTreeCascVarPosNSigmaProton,"fTreeCascVarPosNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarBachNSigmaPion",&fTreeCascVarBachNSigmaPion,"fTreeCascVarBachNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarBachNSigmaKaon",&fTreeCascVarBachNSigmaKaon,"fTreeCascVarBachNSigmaKaon/F");
        
        fTreeCascade->Branch("fTreeCascVarNegTOFNSigmaPion",&fTreeCascVarNegTOFNSigmaPion,"fTreeCascVarNegTOFNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarNegTOFNSigmaProton",&fTreeCascVarNegTOFNSigmaProton,"fTreeCascVarTOFNegNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarPosTOFNSigmaPion",&fTreeCascVarPosTOFNSigmaPion,"fTreeCascVarPosTOFNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarPosTOFNSigmaProton",&fTreeCascVarPosTOFNSigmaProton,"fTreeCascVarPosTOFNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarBachTOFNSigmaPion",&fTreeCascVarBachTOFNSigmaPion,"fTreeCascVarBachTOFNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarBachTOFNSigmaKaon",&fTreeCascVarBachTOFNSigmaKaon,"fTreeCascVarBachTOFNSigmaKaon/F");
        
        fTreeCascade->Branch("fTreeCascVarNegITSNSigmaPion",&fTreeCascVarNegITSNSigmaPion,"fTreeCascVarNegITSNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarNegITSNSigmaProton",&fTreeCascVarNegITSNSigmaProton,"fTreeCascVarITSNegNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarPosITSNSigmaPion",&fTreeCascVarPosITSNSigmaPion,"fTreeCascVarPosITSNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarPosITSNSigmaProton",&fTreeCascVarPosITSNSigmaProton,"fTreeCascVarPosITSNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarBachITSNSigmaPion",&fTreeCascVarBachITSNSigmaPion,"fTreeCascVarBachITSNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarBachITSNSigmaKaon",&fTreeCascVarBachITSNSigmaKaon,"fTreeCascVarBachITSNSigmaKaon/F");
        
        //------------------------------------------------
        fTreeCascade->Branch("fTreeCascVarChiSquareV0",&fTreeCascVarChiSquareV0,"fTreeCascVarChiSquareV0/F");
        fTreeCascade->Branch("fTreeCascVarChiSquareCascade",&fTreeCascVarChiSquareCascade,"fTreeCascVarChiSquareCascade/F");
        //------------------------------------------------
        if ( fkDebugWrongPIDForTracking ){
            fTreeCascade->Branch("fTreeCascVarPosPIDForTracking",&fTreeCascVarPosPIDForTracking,"fTreeCascVarPosPIDForTracking/I");
            fTreeCascade->Branch("fTreeCascVarNegPIDForTracking",&fTreeCascVarNegPIDForTracking,"fTreeCascVarNegPIDForTracking/I");
            fTreeCascade->Branch("fTreeCascVarBachPIDForTracking",&fTreeCascVarBachPIDForTracking,"fTreeCascVarBachPIDForTracking/I");
            fTreeCascade->Branch("fTreeCascVarPosdEdx",&fTreeCascVarPosdEdx,"fTreeCascVarPosdEdx/F");
            fTreeCascade->Branch("fTreeCascVarNegdEdx",&fTreeCascVarNegdEdx,"fTreeCascVarNegdEdx/F");
            fTreeCascade->Branch("fTreeCascVarBachdEdx",&fTreeCascVarBachdEdx,"fTreeCascVarBachdEdx/F");
            fTreeCascade->Branch("fTreeCascVarPosInnerP",&fTreeCascVarPosInnerP,"fTreeCascVarPosInnerP/F");
            fTreeCascade->Branch("fTreeCascVarNegInnerP",&fTreeCascVarNegInnerP,"fTreeCascVarNegInnerP/F");
            fTreeCascade->Branch("fTreeCascVarBachInnerP",&fTreeCascVarBachInnerP,"fTreeCascVarBachInnerP/F");
            fTreeCascade->Branch("fTreeCascVarNegTrackStatus",&fTreeCascVarNegTrackStatus,"fTreeCascVarNegTrackStatus/l");
            fTreeCascade->Branch("fTreeCascVarPosTrackStatus",&fTreeCascVarPosTrackStatus,"fTreeCascVarPosTrackStatus/l");
            fTreeCascade->Branch("fTreeCascVarBachTrackStatus",&fTreeCascVarBachTrackStatus,"fTreeCascVarBachTrackStatus/l");
            fTreeCascade->Branch("fTreeCascVarNegDCAz",&fTreeCascVarNegDCAz,"fTreeCascVarNegDCAz/F");
            fTreeCascade->Branch("fTreeCascVarPosDCAz",&fTreeCascVarPosDCAz,"fTreeCascVarPosDCAz/F");
            fTreeCascade->Branch("fTreeCascVarBachDCAz",&fTreeCascVarBachDCAz,"fTreeCascVarBachDCAz/F");
        }
        //------------------------------------------------
        if ( fkDebugBump ){
            //Variables for debugging the invariant mass bump
            //Full momentum information
            fTreeCascade->Branch("fTreeCascVarPosPx",&fTreeCascVarPosPx,"fTreeCascVarPosPx/F");
            fTreeCascade->Branch("fTreeCascVarPosPy",&fTreeCascVarPosPy,"fTreeCascVarPosPy/F");
            fTreeCascade->Branch("fTreeCascVarPosPz",&fTreeCascVarPosPz,"fTreeCascVarPosPz/F");
            fTreeCascade->Branch("fTreeCascVarNegPx",&fTreeCascVarNegPx,"fTreeCascVarNegPx/F");
            fTreeCascade->Branch("fTreeCascVarNegPy",&fTreeCascVarNegPy,"fTreeCascVarNegPy/F");
            fTreeCascade->Branch("fTreeCascVarNegPz",&fTreeCascVarNegPz,"fTreeCascVarNegPz/F");
            fTreeCascade->Branch("fTreeCascVarBachPx",&fTreeCascVarBachPx,"fTreeCascVarBachPx/F");
            fTreeCascade->Branch("fTreeCascVarBachPy",&fTreeCascVarBachPy,"fTreeCascVarBachPy/F");
            fTreeCascade->Branch("fTreeCascVarBachPz",&fTreeCascVarBachPz,"fTreeCascVarBachPz/F");
            // Decay positions
            fTreeCascade->Branch("fTreeCascVarV0DecayX",&fTreeCascVarV0DecayX,"fTreeCascVarV0DecayX/F");
            fTreeCascade->Branch("fTreeCascVarV0DecayY",&fTreeCascVarV0DecayY,"fTreeCascVarV0DecayY/F");
            fTreeCascade->Branch("fTreeCascVarV0DecayZ",&fTreeCascVarV0DecayZ,"fTreeCascVarV0DecayZ/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayX",&fTreeCascVarCascadeDecayX,"fTreeCascVarCascadeDecayX/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayY",&fTreeCascVarCascadeDecayY,"fTreeCascVarCascadeDecayY/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayZ",&fTreeCascVarCascadeDecayZ,"fTreeCascVarCascadeDecayZ/F");
            
            fTreeCascade->Branch("fTreeCascVarV0Lifetime",&fTreeCascVarV0Lifetime,"fTreeCascVarV0Lifetime/F");
            
            //Track Labels (check for duplicates, etc)
            fTreeCascade->Branch("fTreeCascVarNegIndex",&fTreeCascVarNegIndex,"fTreeCascVarNegIndex/I");
            fTreeCascade->Branch("fTreeCascVarPosIndex",&fTreeCascVarPosIndex,"fTreeCascVarPosIndex/I");
            fTreeCascade->Branch("fTreeCascVarBachIndex",&fTreeCascVarBachIndex,"fTreeCascVarBachIndex/I");
            //Event Number (check same-event index mixups)
            fTreeCascade->Branch("fTreeCascVarEventNumber",&fTreeCascVarEventNumber,"fTreeCascVarEventNumber/l");
        }
        if ( fkDebugOOBPileup ) {
            fTreeCascade->Branch("fTreeCascVarNegTOFExpTDiff",&fTreeCascVarNegTOFExpTDiff,"fTreeCascVarNegTOFExpTDiff/F");
            fTreeCascade->Branch("fTreeCascVarPosTOFExpTDiff",&fTreeCascVarPosTOFExpTDiff,"fTreeCascVarPosTOFExpTDiff/F");
            fTreeCascade->Branch("fTreeCascVarBachTOFExpTDiff",&fTreeCascVarBachTOFExpTDiff,"fTreeCascVarBachTOFExpTDiff/F");
            fTreeCascade->Branch("fTreeCascVarNegTOFSignal",&fTreeCascVarNegTOFSignal,"fTreeCascVarNegTOFSignal/F");
            fTreeCascade->Branch("fTreeCascVarPosTOFSignal",&fTreeCascVarPosTOFSignal,"fTreeCascVarPosTOFSignal/F");
            fTreeCascade->Branch("fTreeCascVarBachTOFSignal",&fTreeCascVarBachTOFSignal,"fTreeCascVarBachTOFSignal/F");
            // Event info
            fTreeCascade->Branch("fTreeCascVarOOBPileupFlag",&fTreeCascVarOOBPileupFlag,"fTreeCascVarOOBPileupFlag/O");
            fTreeCascade->Branch("fTreeCascVarAmplitudeV0A",&fTreeCascVarAmplitudeV0A,"fTreeCascVarAmplitudeV0A/F");
            fTreeCascade->Branch("fTreeCascVarAmplitudeV0C",&fTreeCascVarAmplitudeV0C,"fTreeCascVarAmplitudeV0C/F");
            fTreeCascade->Branch("fTreeCascVarNHitsFMDA",&fTreeCascVarNHitsFMDA,"fTreeCascVarNHitsFMDA/F");
            fTreeCascade->Branch("fTreeCascVarNHitsFMDC",&fTreeCascVarNHitsFMDC,"fTreeCascVarNHitsFMDC/F");
            fTreeCascade->Branch("fTreeCascVarClosestNonEmptyBC",&fTreeCascVarClosestNonEmptyBC,"fTreeCascVarClosestNonEmptyBC/I");
        }
        
        //Kink tagging
        fTreeCascade->Branch("fTreeCascVarBachIsKink",&fTreeCascVarBachIsKink,"fTreeCascVarBachIsKink/O");
        fTreeCascade->Branch("fTreeCascVarPosIsKink",&fTreeCascVarPosIsKink,"fTreeCascVarPosIsKink/O");
        fTreeCascade->Branch("fTreeCascVarNegIsKink",&fTreeCascVarNegIsKink,"fTreeCascVarNegIsKink/O");
        //------------------------------------------------
    }
    //------------------------------------------------
    // Particle Identification Setup
    //------------------------------------------------
    
    AliAnalysisManager *man=AliAnalysisManager::GetAnalysisManager();
    AliInputEventHandler* inputHandler = (AliInputEventHandler*) (man->GetInputEventHandler());
    fPIDResponse = inputHandler->GetPIDResponse();
    inputHandler->SetNeedField();
    
    // Multiplicity
    if(! fESDtrackCuts ) {
        fESDtrackCuts = AliESDtrackCuts::GetStandardITSTPCTrackCuts2010(kTRUE,kFALSE);
        fESDtrackCuts->SetPtRange(0.15);  // adding pt cut
        fESDtrackCuts->SetEtaRange(-1.0, 1.0);
    }
    //Analysis Utils
    if(! fUtils ) {
        fUtils = new AliAnalysisUtils();
    }
    if(! fRand ){
        fRand = new TRandom3();
        // From TRandom3 reference:
        // if seed is 0 (default value) a TUUID is generated and
        // used to fill the first 8 integers of the seed array
        fRand->SetSeed(0);
    }
    
    // OOB Pileup in pp 2016
    if( !fESDtrackCutsGlobal2015 && fkDebugOOBPileup ) {
        fESDtrackCutsGlobal2015 = AliESDtrackCuts::GetStandardITSTPCTrackCuts2015PbPb(kTRUE,kFALSE);
        //Initial set of cuts - to be adjusted
        fESDtrackCutsGlobal2015->SetPtRange(0.15);
        fESDtrackCutsGlobal2015->SetEtaRange(-1.0, 1.0);
    }
    if( !fESDtrackCutsITSsa2010 && fkDebugOOBPileup ) {
        fESDtrackCutsITSsa2010 = AliESDtrackCuts::GetStandardITSSATrackCuts2010();
    }
    
    //------------------------------------------------
    // V0 Multiplicity Histograms
    //------------------------------------------------
    
    // Create histograms
    fListHist = new TList();
    fListHist->SetOwner();  // See http://root.cern.ch/root/html/TCollection.html#TCollection:SetOwner
    
    fEventCuts.AddQAplotsToList(fListHist);
    
    if(! fHistEventCounter ) {
        //Histogram Output: Event-by-Event
        fHistEventCounter = new TH1D( "fHistEventCounter", ";Evt. Sel. Step;Count",2,0,2);
        fHistEventCounter->GetXaxis()->SetBinLabel(1, "Processed");
        fHistEventCounter->GetXaxis()->SetBinLabel(2, "Selected");
        fListHist->Add(fHistEventCounter);
    }
    
    if(! fHistCentrality ) {
        //Histogram Output: Event-by-Event
        fHistCentrality = new TH1D( "fHistCentrality", "WARNING: no pileup rejection applied!;Centrality;Event Count",100,0,100);
        fListHist->Add(fHistCentrality);
    }
    
    //Superlight mode output
    if ( !fListV0 ){
        fListV0 = new TList();
        fListV0->SetOwner();
    }
    
    if ( !fListCascade ){
        //Superlight mode output
        fListCascade = new TList();
        fListCascade->SetOwner();
    }
    
    //Regular Output: Slots 1, 2, 3
    PostData(1, fListHist    );
    PostData(2, fListV0      );
    PostData(3, fListCascade );
    
    //TTree Objects: Slots 4, 5, 6
    if(fkSaveEventTree)    PostData(4, fTreeEvent   );
    if(fkSaveV0Tree)       PostData(5, fTreeV0      );
    if(fkSaveCascadeTree)  PostData(6, fTreeCascade );
    
}// end UserCreateOutputObjects


//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::UserExec(Option_t *)
{
    // Main loop
    // Called for each event
    
    AliESDEvent *lESDevent = 0x0;
    
    // Connect to the InputEvent
    // After these lines, we should have an ESD/AOD event + the number of V0s in it.
    
    // Appropriate for ESD analysis!
    
    lESDevent = dynamic_cast<AliESDEvent*>( InputEvent() );
    if (!lESDevent) {
        AliWarning("ERROR: lESDevent not available \n");
        return;
    }
    
    //Get VZERO Information for multiplicity later
    AliVVZERO* esdV0 = lESDevent->GetVZEROData();
    if (!esdV0) {
        AliError("AliVVZERO not available");
        return;
    }
    
    Double_t lMagneticField = -10;
    lMagneticField = lESDevent->GetMagneticField( );
    
    //------------------------------------------------
    // Retrieving IR info for OOB Pileup rejection
    //------------------------------------------------
    if( fkDebugOOBPileup ) {
        fClosestNonEmptyBC = 10*3564; // start with an isolated event
        AliESDHeader* lESDHeader = (AliESDHeader*)lESDevent->GetHeader();
        Int_t    nIRs       = lESDHeader->GetTriggerIREntries();
        Long64_t lThisOrbit = lESDHeader->GetOrbitNumber();
        Int_t    lThisBC    = lESDHeader->GetBunchCrossNumber();
        
        const AliTriggerIR* lIR;
        for(Int_t i=0; i<nIRs; i++) {
            
            lIR = lESDHeader->GetTriggerIR(i);
            Long64_t lOrbit     = lIR->GetOrbit();
            UInt_t   lNWord     = lIR->GetNWord();
            UShort_t *lBCsForIR = lIR->GetBCs();
            Bool_t   *lInt1     = lIR->GetInt1s();
            Bool_t   *lInt2     = lIR->GetInt2s();
            
            for(UInt_t j=0; j<lNWord; j++) {
                
                if( (lInt1[j]) || (lInt2[j]) ) {
                    
                    Int_t lBC = lBCsForIR[j];
                    
                    if((lOrbit == lThisOrbit) && (lBC != lThisBC)) {
                        Int_t lClosestNonEmptyBC = lBC - lThisBC;
                        if(TMath::Abs(lClosestNonEmptyBC)<TMath::Abs(fClosestNonEmptyBC)) fClosestNonEmptyBC = lClosestNonEmptyBC;
                    }
                    
                    if(lOrbit == (lThisOrbit+1)) {
                        Int_t lClosestNonEmptyBC = (lBC+3564) - lThisBC;
                        if(TMath::Abs(lClosestNonEmptyBC)<TMath::Abs(fClosestNonEmptyBC)) fClosestNonEmptyBC = lClosestNonEmptyBC;
                    }
                    
                    if(lOrbit == (lThisOrbit-1)) {
                        Int_t lClosestNonEmptyBC = (lBC-3564) - lThisBC;
                        if(TMath::Abs(lClosestNonEmptyBC)<TMath::Abs(fClosestNonEmptyBC)) fClosestNonEmptyBC = lClosestNonEmptyBC;
                    }
                }
            }
        }
    }
    // done with IR ----------------------------------
    
    //------------------------------------------------
    // Event Selection ---
    //  --- Performed entirely via AliPPVsMultUtils
    // (except removal of incomplete events and SPDClusterVsTracklets cut)
    //------------------------------------------------
    
    //Copy-paste of steps done in AliAnalysisTaskSkeleton
    
    fHistEventCounter->Fill(0.5);
    
    //------------------------------------------------
    // Primary Vertex Requirements Section:
    //  ---> pp: has vertex, |z|<10cm
    //------------------------------------------------
    
    //classical Proton-proton like selection
    const AliESDVertex *lPrimaryBestESDVtx     = lESDevent->GetPrimaryVertex();
    const AliESDVertex *lPrimaryTrackingESDVtx = lESDevent->GetPrimaryVertexTracks();
    const AliESDVertex *lPrimarySPDVtx         = lESDevent->GetPrimaryVertexSPD();
    
    Double_t lBestPrimaryVtxPos[3]          = {-100.0, -100.0, -100.0};
    lPrimaryBestESDVtx->GetXYZ( lBestPrimaryVtxPos );
    
    //------------------------------------------------
    // Multiplicity Information Acquistion
    //------------------------------------------------
    
    Float_t lPercentile = 500;
    Int_t lEvSelCode = 100;
    AliMultSelection *MultSelection = (AliMultSelection*) lESDevent -> FindListObject("MultSelection");
    if( !MultSelection) {
        //If you get this warning (and lPercentiles 300) please check that the AliMultSelectionTask actually ran (before your task)
        AliWarning("AliMultSelection object not found!");
    } else {
        //V0M Multiplicity Percentile
        lPercentile = MultSelection->GetMultiplicityPercentile("V0M");
        //Event Selection Code
        lEvSelCode = MultSelection->GetEvSelCode();
    }
    
    //just ask AliMultSelection. It will know.
    fMVPileupFlag = kFALSE;
    fMVPileupFlag = MultSelection->GetThisEventIsNotPileupMV();
    
    fCentrality = lPercentile;
    
    if( lEvSelCode != 0 ) {
        PostData(1, fListHist    );
        PostData(2, fListV0      );
        PostData(3, fListCascade );
        if( fkSaveEventTree   ) PostData(4, fTreeEvent   );
        if( fkSaveV0Tree      ) PostData(5, fTreeV0      );
        if( fkSaveCascadeTree ) PostData(6, fTreeCascade );
        return;
    }
    
    AliVEvent *ev = InputEvent();
    if( fkDoExtraEvSels ) {
        if( !fEventCuts.AcceptEvent(ev) ) {
            PostData(1, fListHist    );
            PostData(2, fListV0      );
            PostData(3, fListCascade );
            if( fkSaveEventTree   ) PostData(4, fTreeEvent   );
            if( fkSaveV0Tree      ) PostData(5, fTreeV0      );
            if( fkSaveCascadeTree ) PostData(6, fTreeCascade );
            return;
        }
    }
    
    fHistEventCounter->Fill(1.5);
    
    //Bookkeep event number for debugging
    fTreeCascVarEventNumber =
    ( ( ((ULong64_t)lESDevent->GetPeriodNumber() ) << 36 ) |
     ( ((ULong64_t)lESDevent->GetOrbitNumber () ) << 12 ) |
     ((ULong64_t)lESDevent->GetBunchCrossNumber() )  );
    
    //Save info for pileup study (high multiplicity triggers pp 13 TeV - 2016 data)
    if( fkDebugOOBPileup ) {
        fOOBPileupFlag     = !fUtils->IsOutOfBunchPileUp(ev);
        fNTOFClusters      = lESDevent->GetESDTOFClusters()->GetEntriesFast();
        fNTOFMatches       = lESDevent->GetESDTOFMatches()->GetEntriesFast();
        fNTracksITSsa2010  = 0;
        fNTracksGlobal2015 = 0;
        fNTracksGlobal2015TriggerPP = 0;
        //Count tracks with various selections
        for(Long_t itrack = 0; itrack<lESDevent->GetNumberOfTracks(); itrack++) {
            AliVTrack *track = lESDevent -> GetVTrack( itrack );
            if( !track ) continue;
            //Only ITSsa tracks
            if( fESDtrackCutsITSsa2010->AcceptVTrack(track) ) fNTracksITSsa2010++;
            if( !fESDtrackCutsGlobal2015->AcceptVTrack(track) ) continue;
            //Only for accepted tracks
            fNTracksGlobal2015++;
            //Count accepted + TOF time window (info from Alberica)
            //Warning: 12.5 is appropriate for pp (for Pb-Pb use 30)
            if( TMath::Abs( track->GetTOFExpTDiff() ) < 12.5 ) fNTracksGlobal2015TriggerPP++;
        }
        
        //VZERO info
        fAmplitudeV0A = ((AliMultEstimator*)MultSelection->GetEstimator("V0A"))->GetValue();
        fAmplitudeV0C = ((AliMultEstimator*)MultSelection->GetEstimator("V0C"))->GetValue();
        
        //FMD info
        AliAODEvent* aodEvent = AliForwardUtil::GetAODEvent(this);
        if (!aodEvent) return;
        FMDhits fmdhits = GetFMDhits(aodEvent);
        fNHitsFMDA = std::accumulate(fmdhits.begin(), fmdhits.end(), 0,
                                     [](Float_t a, AliAnalysisTaskStrangenessVsMultiplicityRun2::FMDhit t) {
                                         return a + ((2.8 < t.eta && t.eta < 5.03) ? t.weight : 0.0f);
                                     });
        fNHitsFMDC = std::accumulate(fmdhits.begin(), fmdhits.end(), 0,
                                     [](Float_t a, AliAnalysisTaskStrangenessVsMultiplicityRun2::FMDhit t) {
                                         return a + ((-3.4 < t.eta && t.eta < 2.01) ? t.weight : 0.0f);
                                     });
    }
    
    //Fill centrality histogram
    fHistCentrality->Fill(fCentrality);
    
    //Event-level fill
    if ( fkSaveEventTree ) fTreeEvent->Fill() ;
    
    //STOP HERE if skipping event selections (no point in doing the rest...)
    
    //------------------------------------------------
    
    //------------------------------------------------
    // Fill V0 Tree as needed
    //------------------------------------------------
    
    //Variable definition
    Int_t    lOnFlyStatus = 0;// nv0sOn = 0, nv0sOff = 0;
    Double_t lChi2V0 = 0;
    Double_t lDcaV0Daughters = 0, lDcaV0ToPrimVertex = 0;
    Double_t lDcaPosToPrimVertex = 0, lDcaNegToPrimVertex = 0;
    Double_t lV0CosineOfPointingAngle = 0;
    Double_t lV0Radius = 0, lPt = 0;
    Double_t lRapK0Short = 0, lRapLambda = 0;
    Double_t lInvMassK0s = 0, lInvMassLambda = 0, lInvMassAntiLambda = 0;
    Double_t lAlphaV0 = 0, lPtArmV0 = 0;
    
    Double_t fMinV0Pt = 0;
    Double_t fMaxV0Pt = 100;
    
    //------------------------------------------------
    // Rerun V0 Vertexer!
    // WARNING: this will only work if the
    // special "use on the fly cascading" flag
    // is disabled!
    //------------------------------------------------
    
    if( fkRunVertexers && !fkUseOnTheFlyV0Cascading ) {
        //Only reset if not using on-the-fly (or else nothing passes)
        lESDevent->ResetV0s();
        
        //Decide between regular and light vertexer (default: light)
        if ( ! fkUseLightVertexer ){
            //Instantiate vertexer object
            AliV0vertexer lV0vtxer;
            //Set Cuts
            lV0vtxer.SetDefaultCuts(fV0VertexerSels);
            lV0vtxer.SetCuts(fV0VertexerSels);
            //Redo
            lV0vtxer.Tracks2V0vertices(lESDevent);
        } else {
            //Instantiate vertexer object
            AliLightV0vertexer lV0vtxer;
            //Set do or don't do V0 refit for improved precision
            lV0vtxer.SetDoRefit( kFALSE );
            if (fkDoV0Refit) lV0vtxer.SetDoRefit(kTRUE);
            //Set Cuts
            lV0vtxer.SetDefaultCuts(fV0VertexerSels);
            lV0vtxer.SetCuts(fV0VertexerSels);
            //Redo
            lV0vtxer.Tracks2V0vertices(lESDevent);
        }
    }
    
    Int_t nv0s = 0;
    nv0s = lESDevent->GetNumberOfV0s();
    
    for (Int_t iV0 = 0; iV0 < nv0s; iV0++) //extra-crazy test
    {   // This is the begining of the V0 loop
        AliESDv0 *v0 = ((AliESDEvent*)lESDevent)->GetV0(iV0);
        if (!v0) continue;
        
        CheckChargeV0( v0 );
        //Remove like-sign (will not affect offline V0 candidates!)
        if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() > 0 ){
            continue;
        }
        if( v0->GetParamN()->Charge() < 0 && v0->GetParamP()->Charge() < 0 ){
            continue;
        }
        
        Double_t tDecayVertexV0[3];
        v0->GetXYZ(tDecayVertexV0[0],tDecayVertexV0[1],tDecayVertexV0[2]);
        
        Double_t tV0mom[3];
        v0->GetPxPyPz( tV0mom[0],tV0mom[1],tV0mom[2] );
        Double_t lV0TotalMomentum = TMath::Sqrt(
                                                tV0mom[0]*tV0mom[0]+tV0mom[1]*tV0mom[1]+tV0mom[2]*tV0mom[2] );
        
        lV0Radius = TMath::Sqrt(tDecayVertexV0[0]*tDecayVertexV0[0]+tDecayVertexV0[1]*tDecayVertexV0[1]);
        
        lPt = v0->Pt();
        lRapK0Short = v0->RapK0Short();
        lRapLambda  = v0->RapLambda();
        if ((lPt<fMinV0Pt)||(fMaxV0Pt<lPt)) continue;
        
        UInt_t lKeyPos = (UInt_t)TMath::Abs(v0->GetPindex());
        UInt_t lKeyNeg = (UInt_t)TMath::Abs(v0->GetNindex());
        
        Double_t lMomPos[3];
        v0->GetPPxPyPz(lMomPos[0],lMomPos[1],lMomPos[2]);
        Double_t lMomNeg[3];
        v0->GetNPxPyPz(lMomNeg[0],lMomNeg[1],lMomNeg[2]);
        
        AliESDtrack *pTrack=((AliESDEvent*)lESDevent)->GetTrack(lKeyPos);
        AliESDtrack *nTrack=((AliESDEvent*)lESDevent)->GetTrack(lKeyNeg);
        if (!pTrack || !nTrack) {
            Printf("ERROR: Could not retreive one of the daughter track");
            continue;
        }
        fTreeVariablePosPIDForTracking = pTrack->GetPIDForTracking();
        fTreeVariableNegPIDForTracking = nTrack->GetPIDForTracking();
        
        const AliExternalTrackParam *innernegv0=nTrack->GetInnerParam();
        const AliExternalTrackParam *innerposv0=pTrack->GetInnerParam();
        Float_t lThisPosInnerP = -1;
        Float_t lThisNegInnerP = -1;
        Float_t lThisPosInnerPt = -1;
        Float_t lThisNegInnerPt = -1;
        if(innerposv0)  { lThisPosInnerP  = innerposv0 ->GetP(); }
        if(innernegv0)  { lThisNegInnerP  = innernegv0 ->GetP(); }
        if(innerposv0)  { lThisPosInnerPt  = innerposv0 ->Pt(); }
        if(innernegv0)  { lThisNegInnerPt  = innernegv0 ->Pt(); }
        Float_t lThisPosdEdx = pTrack -> GetTPCsignal();
        Float_t lThisNegdEdx = nTrack -> GetTPCsignal();
        
        fTreeVariablePosdEdx = lThisPosdEdx;
        fTreeVariableNegdEdx = lThisNegdEdx;
        
        fTreeVariablePosInnerP = lThisPosInnerP;
        fTreeVariableNegInnerP = lThisNegInnerP;
        
        //Daughter Eta for Eta selection, afterwards
        fTreeVariableNegEta = nTrack->Eta();
        fTreeVariablePosEta = pTrack->Eta();
        
        if ( fkExtraCleanup ){
            if( TMath::Abs(fTreeVariableNegEta)>0.8 || TMath::Abs(fTreeVariableNegEta)>0.8 ) continue;
            if( TMath::Abs(lRapK0Short        )>0.5 && TMath::Abs(lRapLambda         )>0.5 ) continue;
        }
        
        // Filter like-sign V0 (next: add counter and distribution)
        if ( pTrack->GetSign() == nTrack->GetSign()) {
            continue;
        }
        
        //________________________________________________________________________
        // Track quality cuts
        Float_t lPosTrackCrossedRows = pTrack->GetTPCClusterInfo(2,1);
        Float_t lNegTrackCrossedRows = nTrack->GetTPCClusterInfo(2,1);
        fTreeVariableLeastNbrCrossedRows = (Int_t) lPosTrackCrossedRows;
        if( lNegTrackCrossedRows < fTreeVariableLeastNbrCrossedRows )
        fTreeVariableLeastNbrCrossedRows = (Int_t) lNegTrackCrossedRows;
        
        // TPC refit condition (done during reconstruction for Offline but not for On-the-fly)
        if( !(pTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
        if( !(nTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
        
        //Get status flags
        fTreeVariablePosTrackStatus = pTrack->GetStatus();
        fTreeVariableNegTrackStatus = nTrack->GetStatus();
        
        fTreeVariablePosDCAz = GetDCAz(pTrack);
        fTreeVariableNegDCAz = GetDCAz(nTrack);
        
        //GetKinkIndex condition
        if( pTrack->GetKinkIndex(0)>0 || nTrack->GetKinkIndex(0)>0 ) continue;
        
        //Findable clusters > 0 condition
        if( pTrack->GetTPCNclsF()<=0 || nTrack->GetTPCNclsF()<=0 ) continue;
        
        //Compute ratio Crossed Rows / Findable clusters
        //Note: above test avoids division by zero!
        Float_t lPosTrackCrossedRowsOverFindable = lPosTrackCrossedRows / ((double)(pTrack->GetTPCNclsF()));
        Float_t lNegTrackCrossedRowsOverFindable = lNegTrackCrossedRows / ((double)(nTrack->GetTPCNclsF()));
        
        fTreeVariableLeastRatioCrossedRowsOverFindable = lPosTrackCrossedRowsOverFindable;
        if( lNegTrackCrossedRowsOverFindable < fTreeVariableLeastRatioCrossedRowsOverFindable )
        fTreeVariableLeastRatioCrossedRowsOverFindable = lNegTrackCrossedRowsOverFindable;
        
        //Lowest Cut Level for Ratio Crossed Rows / Findable = 0.8, set here
        //if ( fTreeVariableLeastRatioCrossedRowsOverFindable < 0.8 ) continue;
        
        //Extra track quality: Chi2/cluster for cross-checks
        Float_t lBiggestChi2PerCluster = -1;
        
        Float_t lPosChi2PerCluster = 1000;
        Float_t lNegChi2PerCluster = 1000;
        
        if( pTrack->GetTPCNcls() > 0 ) lPosChi2PerCluster = pTrack->GetTPCchi2() / ((Float_t)pTrack->GetTPCNcls());
        if( nTrack->GetTPCNcls() > 0 ) lNegChi2PerCluster = nTrack->GetTPCchi2() / ((Float_t)nTrack->GetTPCNcls());
        
        if ( lPosChi2PerCluster  > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lPosChi2PerCluster;
        if ( lNegChi2PerCluster  > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lNegChi2PerCluster;
        
        fTreeVariableMaxChi2PerCluster = lBiggestChi2PerCluster;
        
        //Extra track quality: min track length
        Float_t lSmallestTrackLength = 1000;
        Float_t lPosTrackLength = -1;
        Float_t lNegTrackLength = -1;
        
        if (pTrack->GetInnerParam()) lPosTrackLength = pTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (nTrack->GetInnerParam()) lNegTrackLength = nTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        
        if ( lPosTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lPosTrackLength;
        if ( lNegTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lNegTrackLength;
        
        fTreeVariableMinTrackLength = lSmallestTrackLength;
        
        if ( ( ( ( pTrack->GetTPCClusterInfo(2,1) ) < 70 ) || ( ( nTrack->GetTPCClusterInfo(2,1) ) < 70 ) ) && lSmallestTrackLength<80 ) continue;
        
        //End track Quality Cuts
        //________________________________________________________________________
        
        lDcaPosToPrimVertex = TMath::Abs(pTrack->GetD(lBestPrimaryVtxPos[0],
                                                      lBestPrimaryVtxPos[1],
                                                      lMagneticField) );
        
        lDcaNegToPrimVertex = TMath::Abs(nTrack->GetD(lBestPrimaryVtxPos[0],
                                                      lBestPrimaryVtxPos[1],
                                                      lMagneticField) );
        
        lOnFlyStatus = v0->GetOnFlyStatus();
        lChi2V0 = v0->GetChi2V0();
        lDcaV0Daughters = v0->GetDcaV0Daughters();
        lDcaV0ToPrimVertex = v0->GetD(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
        lV0CosineOfPointingAngle = v0->GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
        fTreeVariableV0CosineOfPointingAngle=lV0CosineOfPointingAngle;
        
        // Getting invariant mass infos directly from ESD
        v0->ChangeMassHypothesis(310);
        lInvMassK0s = v0->GetEffMass();
        v0->ChangeMassHypothesis(3122);
        lInvMassLambda = v0->GetEffMass();
        v0->ChangeMassHypothesis(-3122);
        lInvMassAntiLambda = v0->GetEffMass();
        lAlphaV0 = v0->AlphaV0();
        lPtArmV0 = v0->PtArmV0();
        
        fTreeVariableMVPileupFlag = fMVPileupFlag;
        
        fTreeVariablePt = v0->Pt();
        fTreeVariableChi2V0 = lChi2V0;
        fTreeVariableDcaV0ToPrimVertex = lDcaV0ToPrimVertex;
        fTreeVariableDcaV0Daughters = lDcaV0Daughters;
        fTreeVariableV0CosineOfPointingAngle = lV0CosineOfPointingAngle;
        fTreeVariableV0Radius = lV0Radius;
        fTreeVariableDcaPosToPrimVertex = lDcaPosToPrimVertex;
        fTreeVariableDcaNegToPrimVertex = lDcaNegToPrimVertex;
        fTreeVariableInvMassK0s = lInvMassK0s;
        fTreeVariableInvMassLambda = lInvMassLambda;
        fTreeVariableInvMassAntiLambda = lInvMassAntiLambda;
        fTreeVariableRapK0Short = lRapK0Short;
        fTreeVariableRapLambda = lRapLambda;
        fTreeVariableAlphaV0 = lAlphaV0;
        fTreeVariablePtArmV0 = lPtArmV0;
        
        //Official means of acquiring N-sigmas
        fTreeVariableNSigmasPosProton = fPIDResponse->NumberOfSigmasTPC( pTrack, AliPID::kProton );
        fTreeVariableNSigmasPosPion   = fPIDResponse->NumberOfSigmasTPC( pTrack, AliPID::kPion );
        fTreeVariableNSigmasNegProton = fPIDResponse->NumberOfSigmasTPC( nTrack, AliPID::kProton );
        fTreeVariableNSigmasNegPion   = fPIDResponse->NumberOfSigmasTPC( nTrack, AliPID::kPion );
        
        //This requires an Invariant Mass Hypothesis afterwards
        fTreeVariableDistOverTotMom = TMath::Sqrt(
                                                  TMath::Power( tDecayVertexV0[0] - lBestPrimaryVtxPos[0] , 2) +
                                                  TMath::Power( tDecayVertexV0[1] - lBestPrimaryVtxPos[1] , 2) +
                                                  TMath::Power( tDecayVertexV0[2] - lBestPrimaryVtxPos[2] , 2)
                                                  );
        fTreeVariableDistOverTotMom /= (lV0TotalMomentum+1e-10); //avoid division by zero, to be sure
        
        //Copy Multiplicity information
        fTreeVariableCentrality = fCentrality;
        
        //Info for pileup studies
        if( fkDebugOOBPileup ) {
            fTreeVariableNegTOFExpTDiff = nTrack->GetTOFExpTDiff( lESDevent->GetMagneticField() );
            fTreeVariablePosTOFExpTDiff = pTrack->GetTOFExpTDiff( lESDevent->GetMagneticField() );
            fTreeVariableNegTOFSignal = nTrack->GetTOFsignal() * 1.e-3; // in ns 
            fTreeVariablePosTOFSignal = pTrack->GetTOFsignal() * 1.e-3; // in ns
            //Copy OOB pileup flag for this event
            fTreeVariableOOBPileupFlag = fOOBPileupFlag;
            //Copy VZERO information for this event
            fTreeVariableAmplitudeV0A = fAmplitudeV0A;
            fTreeVariableAmplitudeV0C = fAmplitudeV0C;
            //Copy FMD information for this event
            fTreeVariableNHitsFMDA = fNHitsFMDA;
            fTreeVariableNHitsFMDC = fNHitsFMDC;
            //Copy IR information for this event
            fTreeVariableClosestNonEmptyBC = fClosestNonEmptyBC;
        }
        
        
        //------------------------------------------------
        // Fill Tree!
        //------------------------------------------------
        
        // The conditionals are meant to decrease excessive
        // memory usage!
        
        //First Selection: Reject OnFly
        if( lOnFlyStatus == 0 ) {
            //Second Selection: rough 20-sigma band, parametric.
            //K0Short: Enough to parametrize peak broadening with linear function.
            Double_t lUpperLimitK0Short = (5.63707e-01) + (1.14979e-02)*fTreeVariablePt;
            Double_t lLowerLimitK0Short = (4.30006e-01) - (1.10029e-02)*fTreeVariablePt;
            //Lambda: Linear (for higher pt) plus exponential (for low-pt broadening)
            //[0]+[1]*x+[2]*TMath::Exp(-[3]*x)
            Double_t lUpperLimitLambda = (1.13688e+00) + (5.27838e-03)*fTreeVariablePt + (8.42220e-02)*TMath::Exp(-(3.80595e+00)*fTreeVariablePt);
            Double_t lLowerLimitLambda = (1.09501e+00) - (5.23272e-03)*fTreeVariablePt - (7.52690e-02)*TMath::Exp(-(3.46339e+00)*fTreeVariablePt);
            //Do Selection
            if(
               //Case 1: Lambda Selection
               (fTreeVariableInvMassLambda    < lUpperLimitLambda  && fTreeVariableInvMassLambda     > lLowerLimitLambda &&
                (!fkPreselectDedx || (fkPreselectDedx&&TMath::Abs(fTreeVariableNSigmasPosProton) < 7.0 && TMath::Abs(fTreeVariableNSigmasNegPion) < 7.0) )
                )
               ||
               //Case 2: AntiLambda Selection
               (fTreeVariableInvMassAntiLambda < lUpperLimitLambda  && fTreeVariableInvMassAntiLambda > lLowerLimitLambda &&
                (!fkPreselectDedx || (fkPreselectDedx&&TMath::Abs(fTreeVariableNSigmasNegProton) < 7.0 && TMath::Abs(fTreeVariableNSigmasPosPion) < 7.0) )
                )
               ||
               //Case 3: K0Short Selection
               (fTreeVariableInvMassK0s        < lUpperLimitK0Short && fTreeVariableInvMassK0s        > lLowerLimitK0Short &&
                (!fkPreselectDedx || (fkPreselectDedx&&TMath::Abs(fTreeVariableNSigmasNegPion)   < 7.0 && TMath::Abs(fTreeVariableNSigmasPosPion) < 7.0) )
                ) ) {
                   //Pre-selection in case this is AA...
                   
                   //Random denial
                   Bool_t lKeepV0 = kTRUE;
                   if(fkDownScaleV0 && ( fRand->Uniform() > fDownScaleFactorV0 )) lKeepV0 = kFALSE;
                   
                   //pT window
                   if( fTreeVariablePt < fMinPtToSave ) lKeepV0 = kFALSE;
                   if( fTreeVariablePt > fMaxPtToSave ) lKeepV0 = kFALSE;
                   
                   if ( TMath::Abs(fTreeVariableNegEta)<0.8 && TMath::Abs(fTreeVariablePosEta)<0.8 && fkSaveV0Tree && lKeepV0 ) fTreeV0->Fill();
               }
        }
        
        //------------------------------------------------
        // Fill V0 tree over.
        //------------------------------------------------
        
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //Step 1: Sweep members of the output object TList and fill all of them as appropriate
        Int_t lNumberOfConfigurations = fListV0->GetEntries();
        //AliWarning(Form("[V0 Analyses] Processing different configurations (%i detected)",lNumberOfConfigurations));
        TH3F *histoout         = 0x0;
        AliV0Result *lV0Result = 0x0;
        for(Int_t lcfg=0; lcfg<lNumberOfConfigurations; lcfg++){
            lV0Result = (AliV0Result*) fListV0->At(lcfg);
            histoout  = lV0Result->GetHistogram();
            
            Float_t lMass = 0;
            Float_t lRap  = 0;
            Float_t lPDGMass = -1;
            Float_t lNegdEdx = 100;
            Float_t lPosdEdx = 100;
            Float_t lBaryonMomentum = -0.5;
            Float_t lBaryonPt = -0.5;
            Float_t lBaryondEdxFromProton = 0;
            
            //========================================================================
            //Setting up: Variable V0 CosPA
            Float_t lV0CosPACut = lV0Result -> GetCutV0CosPA();
            Float_t lVarV0CosPApar[5];
            lVarV0CosPApar[0] = lV0Result->GetCutVarV0CosPAExp0Const();
            lVarV0CosPApar[1] = lV0Result->GetCutVarV0CosPAExp0Slope();
            lVarV0CosPApar[2] = lV0Result->GetCutVarV0CosPAExp1Const();
            lVarV0CosPApar[3] = lV0Result->GetCutVarV0CosPAExp1Slope();
            lVarV0CosPApar[4] = lV0Result->GetCutVarV0CosPAConst();
            Float_t lVarV0CosPA = TMath::Cos(
                                             lVarV0CosPApar[0]*TMath::Exp(lVarV0CosPApar[1]*fTreeVariablePt) +
                                             lVarV0CosPApar[2]*TMath::Exp(lVarV0CosPApar[3]*fTreeVariablePt) +
                                             lVarV0CosPApar[4]);
            if( lV0Result->GetCutUseVarV0CosPA() ){
                //Only use if tighter than the non-variable cut
                if( lVarV0CosPA > lV0CosPACut ) lV0CosPACut = lVarV0CosPA;
            }
            //========================================================================
            
            if ( lV0Result->GetMassHypothesis() == AliV0Result::kK0Short     ){
                lMass    = fTreeVariableInvMassK0s;
                lRap     = fTreeVariableRapK0Short;
                lPDGMass = 0.497;
                lNegdEdx = fTreeVariableNSigmasNegPion;
                lPosdEdx = fTreeVariableNSigmasPosPion;
            }
            if ( lV0Result->GetMassHypothesis() == AliV0Result::kLambda      ){
                lMass = fTreeVariableInvMassLambda;
                lRap = fTreeVariableRapLambda;
                lPDGMass = 1.115683;
                lNegdEdx = fTreeVariableNSigmasNegPion;
                lPosdEdx = fTreeVariableNSigmasPosProton;
                lBaryonMomentum = fTreeVariablePosInnerP;
                lBaryonPt = lThisPosInnerPt;
                lBaryondEdxFromProton = fTreeVariableNSigmasPosProton;
            }
            if ( lV0Result->GetMassHypothesis() == AliV0Result::kAntiLambda  ){
                lMass = fTreeVariableInvMassAntiLambda;
                lRap = fTreeVariableRapLambda;
                lPDGMass = 1.115683;
                lNegdEdx = fTreeVariableNSigmasNegProton;
                lPosdEdx = fTreeVariableNSigmasPosPion;
                lBaryonMomentum = fTreeVariableNegInnerP;
                lBaryonPt = lThisNegInnerPt;
                lBaryondEdxFromProton = fTreeVariableNSigmasNegProton;
            }
            
            if (
                //Check 1: Offline Vertexer
                lOnFlyStatus == lV0Result->GetUseOnTheFly() &&
                
                //Check 2: Basic Acceptance cuts
                lV0Result->GetCutMinEtaTracks() < fTreeVariableNegEta && fTreeVariableNegEta < lV0Result->GetCutMaxEtaTracks() &&
                lV0Result->GetCutMinEtaTracks() < fTreeVariablePosEta && fTreeVariablePosEta < lV0Result->GetCutMaxEtaTracks() &&
                lRap > lV0Result->GetCutMinRapidity() &&
                lRap < lV0Result->GetCutMaxRapidity() &&
                
                //Check 3: Topological Variables
                fTreeVariableV0Radius > lV0Result->GetCutV0Radius() &&
                fTreeVariableV0Radius < lV0Result->GetCutMaxV0Radius() &&
                fTreeVariableDcaNegToPrimVertex > lV0Result->GetCutDCANegToPV() &&
                fTreeVariableDcaPosToPrimVertex > lV0Result->GetCutDCAPosToPV() &&
                fTreeVariableDcaV0Daughters < lV0Result->GetCutDCAV0Daughters() &&
                fTreeVariableV0CosineOfPointingAngle > lV0CosPACut &&
                fTreeVariableDistOverTotMom*lPDGMass < lV0Result->GetCutProperLifetime() &&
                fTreeVariableLeastNbrCrossedRows > lV0Result->GetCutLeastNumberOfCrossedRows() &&
                fTreeVariableLeastRatioCrossedRowsOverFindable > lV0Result->GetCutLeastNumberOfCrossedRowsOverFindable() &&
                
                //Check 4: Minimum momentum of baryon daughter
                ( lV0Result->GetMassHypothesis() == AliV0Result::kK0Short || lBaryonMomentum > lV0Result->GetCutMinBaryonMomentum() ) &&
                
                //Check 5: TPC dEdx selections
                TMath::Abs(lNegdEdx)<lV0Result->GetCutTPCdEdx() &&
                TMath::Abs(lPosdEdx)<lV0Result->GetCutTPCdEdx() &&
                
                //Check 6: Armenteros-Podolanski space cut (for K0Short analysis)
                ( ( lV0Result->GetCutArmenteros() == kFALSE || lV0Result->GetMassHypothesis() != AliV0Result::kK0Short ) || ( fTreeVariablePtArmV0>lV0Result->GetCutArmenterosParameter()*TMath::Abs(fTreeVariableAlphaV0) ) ) &&
                
                //Check 7: kITSrefit track selection if requested
                (
                 ( (fTreeVariableNegTrackStatus & AliESDtrack::kITSrefit) &&
                  (fTreeVariablePosTrackStatus & AliESDtrack::kITSrefit) )
                 ||
                 !lV0Result->GetCutUseITSRefitTracks()
                 )&&
                
                //Check 8: Max Chi2/Clusters if not absurd
                ( lV0Result->GetCutMaxChi2PerCluster()>1e+3 ||
                 fTreeVariableMaxChi2PerCluster < lV0Result->GetCutMaxChi2PerCluster()
                 ) &&
                //Check 9: Min Track Length if positive
                ( lV0Result->GetCutMinTrackLength()<0 || //this is a bit paranoid...
                 fTreeVariableMinTrackLength > lV0Result->GetCutMinTrackLength()
                 )&&
                
                //Check 10: Special 2.76TeV-like dedx
                // Logic: either not requested, or K0Short, or high-pT baryon daughter, or passes cut!
                ( !lV0Result->GetCut276TeVLikedEdx() ||
                 ( lV0Result->GetMassHypothesis() == AliV0Result::kK0Short ||
                  ( lBaryonPt > 1.0 || TMath::Abs(lBaryondEdxFromProton)<3.0 )
                  )
                 )
                )
            {
                //This satisfies all my conditionals! Fill histogram
                histoout -> Fill ( fCentrality, fTreeVariablePt, lMass );
            }
        }
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // End Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
    }// This is the end of the V0 loop
    
    //------------------------------------------------
    // Rerun cascade vertexer!
    //------------------------------------------------
    
    if( fkRunVertexers ) {
        //Remove existing cascades
        lESDevent->ResetCascades();
        
        //Decide between regular and light vertexer (default: light)
        if ( ! fkUseLightVertexer ){
            //Instantiate vertexer object
            AliCascadeVertexer lCascVtxer;
            lCascVtxer.SetDefaultCuts(fCascadeVertexerSels);
            lCascVtxer.SetCuts(fCascadeVertexerSels);
            lCascVtxer.V0sTracks2CascadeVertices(lESDevent);
        } else {
            AliLightCascadeVertexer lCascVtxer;
            lCascVtxer.SetDefaultCuts(fCascadeVertexerSels);
            lCascVtxer.SetCuts(fCascadeVertexerSels);
            if( fkUseOnTheFlyV0Cascading ) lCascVtxer.SetUseOnTheFlyV0(kTRUE);
            lCascVtxer.V0sTracks2CascadeVertices(lESDevent);
        }
    }
    
    //------------------------------------------------
    // MAIN CASCADE LOOP STARTS HERE
    //------------------------------------------------
    // Code Credit: Antonin Maire (thanks^100)
    // ---> This is an adaptation
    
    Long_t ncascades = 0;
    ncascades = lESDevent->GetNumberOfCascades();
    
    for (Int_t iXi = 0; iXi < ncascades; iXi++) {
        //------------------------------------------------
        // Initializations
        //------------------------------------------------
        //Double_t lTrkgPrimaryVtxRadius3D = -500.0;
        //Double_t lBestPrimaryVtxRadius3D = -500.0;
        
        // - 1st part of initialisation : variables needed to store AliESDCascade data members
        Double_t lEffMassXi      = 0. ;
        //Double_t lChi2Xi         = -1. ;
        Double_t lDcaXiDaughters = -1. ;
        Double_t lXiCosineOfPointingAngle = -1. ;
        Double_t lPosXi[3] = { -1000.0, -1000.0, -1000.0 };
        Double_t lXiRadius = -1000. ;
        
        // - 2nd part of initialisation : Nbr of clusters within TPC for the 3 daughter cascade tracks
        Int_t    lPosTPCClusters    = -1; // For ESD only ...//FIXME : wait for availability in AOD
        Int_t    lNegTPCClusters    = -1; // For ESD only ...
        Int_t    lBachTPCClusters   = -1; // For ESD only ...
        
        // - 3rd part of initialisation : about V0 part in cascades
        Double_t lInvMassLambdaAsCascDghter = 0.;
        //Double_t lV0Chi2Xi         = -1. ;
        Double_t lDcaV0DaughtersXi = -1.;
        
        Double_t lDcaBachToPrimVertexXi = -1., lDcaV0ToPrimVertexXi = -1.;
        Double_t lDcaPosToPrimVertexXi  = -1.;
        Double_t lDcaNegToPrimVertexXi  = -1.;
        Double_t lV0CosineOfPointingAngleXi = -1. ;
        Double_t lV0CosineOfPointingAngleXiSpecial = -1. ;
        Double_t lPosV0Xi[3] = { -1000. , -1000., -1000. }; // Position of VO coming from cascade
        Double_t lV0RadiusXi = -1000.0;
        Double_t lV0quality  = 0.;
        
        // - 4th part of initialisation : Effective masses
        Double_t lInvMassXiMinus    = 0.;
        Double_t lInvMassXiPlus     = 0.;
        Double_t lInvMassOmegaMinus = 0.;
        Double_t lInvMassOmegaPlus  = 0.;
        
        fTreeCascVarChiSquareV0      = 1e+3;
        fTreeCascVarChiSquareCascade = 1e+3;
        
        // - 6th part of initialisation : extra info for QA
        Double_t lXiMomX       = 0. , lXiMomY = 0., lXiMomZ = 0.;
        Double_t lXiTransvMom  = 0. ;
        //Double_t lXiTransvMomMC= 0. ;
        Double_t lXiTotMom     = 0. ;
        
        Double_t lBachMomX       = 0., lBachMomY  = 0., lBachMomZ   = 0.;
        //Double_t lBachTransvMom  = 0.;
        //Double_t lBachTotMom     = 0.;
        
        fTreeCascVarNegNSigmaPion   = -100;
        fTreeCascVarNegNSigmaProton = -100;
        fTreeCascVarPosNSigmaPion   = -100;
        fTreeCascVarPosNSigmaProton = -100;
        fTreeCascVarBachNSigmaPion  = -100;
        fTreeCascVarBachNSigmaKaon  = -100;
        
        fTreeCascVarNegTOFNSigmaPion   = -100;
        fTreeCascVarNegTOFNSigmaProton = -100;
        fTreeCascVarPosTOFNSigmaPion   = -100;
        fTreeCascVarPosTOFNSigmaProton = -100;
        fTreeCascVarBachTOFNSigmaPion  = -100;
        fTreeCascVarBachTOFNSigmaKaon  = -100;
        
        fTreeCascVarBachIsKink = kFALSE;
        fTreeCascVarPosIsKink = kFALSE;
        fTreeCascVarNegIsKink = kFALSE;
        
        //fTreeCascVarBachTotMom = -1;
        //fTreeCascVarPosTotMom  = -1;
        //fTreeCascVarNegTotMom  = -1;
        
        Short_t  lChargeXi = -2;
        //Double_t lV0toXiCosineOfPointingAngle = 0. ;
        
        Double_t lRapXi   = -20.0, lRapOmega = -20.0; //  lEta = -20.0, lTheta = 360., lPhi = 720. ;
        //Double_t lAlphaXi = -200., lPtArmXi  = -200.0;
        
        // -------------------------------------
        // II.ESD - Calculation Part dedicated to Xi vertices (ESD)
        
        AliESDcascade *xi = lESDevent->GetCascade(iXi);
        if (!xi) continue;
        
        // - II.Step 2 : Assigning the necessary variables for specific AliESDcascade data members (ESD)
        //-------------
        lV0quality = 0.;
        xi->ChangeMassHypothesis(lV0quality , 3312); // default working hypothesis : cascade = Xi- decay
        
        lEffMassXi  			= xi->GetEffMassXi();
        
        //ChiSquare implementation
        fTreeCascVarChiSquareV0      = xi->GetChi2V0();
        fTreeCascVarChiSquareCascade = xi->GetChi2Xi();
        
        lDcaXiDaughters 	= xi->GetDcaXiDaughters();
        lXiCosineOfPointingAngle 	            = xi->GetCascadeCosineOfPointingAngle( lBestPrimaryVtxPos[0],
                                                                                      lBestPrimaryVtxPos[1],
                                                                                      lBestPrimaryVtxPos[2] );
        // Take care : the best available vertex should be used (like in AliCascadeVertexer)
        
        xi->GetXYZcascade( lPosXi[0],  lPosXi[1], lPosXi[2] );
        lXiRadius			= TMath::Sqrt( lPosXi[0]*lPosXi[0]  +  lPosXi[1]*lPosXi[1] );
        
        fTreeCascVarCascadeDecayX = lPosXi[0];
        fTreeCascVarCascadeDecayY = lPosXi[1];
        fTreeCascVarCascadeDecayZ = lPosXi[2];
        
        // - II.Step 3 : around the tracks : Bach + V0 (ESD)
        // ~ Necessary variables for ESDcascade data members coming from the ESDv0 part (inheritance)
        //-------------
        
        UInt_t lIdxPosXi 	= (UInt_t) TMath::Abs( xi->GetPindex() );
        UInt_t lIdxNegXi 	= (UInt_t) TMath::Abs( xi->GetNindex() );
        UInt_t lBachIdx 	= (UInt_t) TMath::Abs( xi->GetBindex() );
        
        // Care track label can be negative in MC production (linked with the track quality)
        // However = normally, not the case for track index ...
        
        // FIXME : rejection of a double use of a daughter track (nothing but just a crosscheck of what is done in the cascade vertexer)
        if(lBachIdx == lIdxNegXi) {
            AliWarning("Pb / Idx(Bach. track) = Idx(Neg. track) ... continue!");
            continue;
        }
        if(lBachIdx == lIdxPosXi) {
            AliWarning("Pb / Idx(Bach. track) = Idx(Pos. track) ... continue!");
            continue;
        }
        
        AliESDtrack *pTrackXi		= lESDevent->GetTrack( lIdxPosXi );
        AliESDtrack *nTrackXi		= lESDevent->GetTrack( lIdxNegXi );
        AliESDtrack *bachTrackXi	= lESDevent->GetTrack( lBachIdx );
        
        fTreeCascVarNegIndex  = lIdxNegXi;
        fTreeCascVarPosIndex  = lIdxPosXi;
        fTreeCascVarBachIndex = lBachIdx;
        
        if (!pTrackXi || !nTrackXi || !bachTrackXi ) {
            AliWarning("ERROR: Could not retrieve one of the 3 ESD daughter tracks of the cascade ...");
            continue;
        }
        
        fTreeCascVarPosEta = pTrackXi->Eta();
        fTreeCascVarNegEta = nTrackXi->Eta();
        fTreeCascVarBachEta = bachTrackXi->Eta();
        
        //GetKinkIndex condition
        if( bachTrackXi->GetKinkIndex(0)>0 ) fTreeCascVarBachIsKink = kTRUE;
        if( pTrackXi->GetKinkIndex(0)>0 ) fTreeCascVarPosIsKink = kTRUE;
        if( nTrackXi->GetKinkIndex(0)>0 ) fTreeCascVarNegIsKink = kTRUE;
        
        //Get track uncertainties
        //WARNING: THIS REFERS TO THE UNCERTAINTIES CLOSEST TO THE PV
        fTreeCascVarNegDCAPVSigmaX2 = TMath::Power(TMath::Sin(nTrackXi->GetAlpha()),2)*nTrackXi->GetSigmaY2();
        fTreeCascVarNegDCAPVSigmaY2 = TMath::Power(TMath::Cos(nTrackXi->GetAlpha()),2)*nTrackXi->GetSigmaY2();
        fTreeCascVarNegDCAPVSigmaZ2 = nTrackXi->GetSigmaZ2();
        
        fTreeCascVarPosDCAPVSigmaX2 = TMath::Power(TMath::Sin(pTrackXi->GetAlpha()),2)*pTrackXi->GetSigmaY2();
        fTreeCascVarPosDCAPVSigmaY2 = TMath::Power(TMath::Cos(pTrackXi->GetAlpha()),2)*pTrackXi->GetSigmaY2();
        fTreeCascVarPosDCAPVSigmaZ2 = pTrackXi->GetSigmaZ2();
        
        fTreeCascVarBachDCAPVSigmaX2 = TMath::Power(TMath::Sin(bachTrackXi->GetAlpha()),2)*bachTrackXi->GetSigmaY2();
        fTreeCascVarBachDCAPVSigmaY2 = TMath::Power(TMath::Cos(bachTrackXi->GetAlpha()),2)*bachTrackXi->GetSigmaY2();
        fTreeCascVarBachDCAPVSigmaZ2 = bachTrackXi->GetSigmaZ2();
        
        Double_t lBMom[3], lNMom[3], lPMom[3];
        xi->GetBPxPyPz( lBMom[0], lBMom[1], lBMom[2] );
        xi->GetPPxPyPz( lPMom[0], lPMom[1], lPMom[2] );
        xi->GetNPxPyPz( lNMom[0], lNMom[1], lNMom[2] );
        
        //fTreeCascVarBachTotMom = TMath::Sqrt( lBMom[0]*lBMom[0] + lBMom[1]*lBMom[1] + lBMom[2]*lBMom[2] );
        //fTreeCascVarPosTotMom  = TMath::Sqrt( lPMom[0]*lPMom[0] + lPMom[1]*lPMom[1] + lPMom[2]*lPMom[2] );
        //fTreeCascVarNegTotMom  = TMath::Sqrt( lNMom[0]*lNMom[0] + lNMom[1]*lNMom[1] + lNMom[2]*lNMom[2] );
        
        fTreeCascVarNegPx = lNMom[0];
        fTreeCascVarNegPy = lNMom[1];
        fTreeCascVarNegPz = lNMom[2];
        fTreeCascVarPosPx = lPMom[0];
        fTreeCascVarPosPy = lPMom[1];
        fTreeCascVarPosPz = lPMom[2];
        fTreeCascVarBachPx = lBMom[0];
        fTreeCascVarBachPy = lBMom[1];
        fTreeCascVarBachPz = lBMom[2];
        
        Int_t lNegTrackSign = 1;
        Int_t lPosTrackSign = 1;
        Int_t lBachTrackSign = 1;
        
        if( nTrackXi->GetSign() < 0 ) lNegTrackSign = -1;
        if( nTrackXi->GetSign() > 0 ) lNegTrackSign = +1;
        
        if( pTrackXi->GetSign() < 0 ) lPosTrackSign = -1;
        if( pTrackXi->GetSign() > 0 ) lPosTrackSign = +1;
        
        if( bachTrackXi->GetSign() < 0 ) lBachTrackSign = -1;
        if( bachTrackXi->GetSign() > 0 ) lBachTrackSign = +1;
        
        //------------------------------------------------
        // TPC dEdx information
        //------------------------------------------------
        fTreeCascVarNegNSigmaPion   = fPIDResponse->NumberOfSigmasTPC( nTrackXi, AliPID::kPion   );
        fTreeCascVarNegNSigmaProton = fPIDResponse->NumberOfSigmasTPC( nTrackXi, AliPID::kProton );
        fTreeCascVarPosNSigmaPion   = fPIDResponse->NumberOfSigmasTPC( pTrackXi, AliPID::kPion );
        fTreeCascVarPosNSigmaProton = fPIDResponse->NumberOfSigmasTPC( pTrackXi, AliPID::kProton );
        fTreeCascVarBachNSigmaPion  = fPIDResponse->NumberOfSigmasTPC( bachTrackXi, AliPID::kPion );
        fTreeCascVarBachNSigmaKaon  = fPIDResponse->NumberOfSigmasTPC( bachTrackXi, AliPID::kKaon );

        //------------------------------------------------
        // TOF info (no correction for weak decay traj.)
        //------------------------------------------------
        fTreeCascVarNegTOFNSigmaPion   = fPIDResponse->NumberOfSigmasTOF( nTrackXi, AliPID::kPion   );
        fTreeCascVarNegTOFNSigmaProton = fPIDResponse->NumberOfSigmasTOF( nTrackXi, AliPID::kProton );
        fTreeCascVarPosTOFNSigmaPion   = fPIDResponse->NumberOfSigmasTOF( pTrackXi, AliPID::kPion );
        fTreeCascVarPosTOFNSigmaProton = fPIDResponse->NumberOfSigmasTOF( pTrackXi, AliPID::kProton );
        fTreeCascVarBachTOFNSigmaPion  = fPIDResponse->NumberOfSigmasTOF( bachTrackXi, AliPID::kPion );
        fTreeCascVarBachTOFNSigmaKaon  = fPIDResponse->NumberOfSigmasTOF( bachTrackXi, AliPID::kKaon );
        
        fTreeCascVarNegITSNSigmaPion   = fPIDResponse->NumberOfSigmasITS( nTrackXi, AliPID::kPion   );
        fTreeCascVarNegITSNSigmaProton = fPIDResponse->NumberOfSigmasITS( nTrackXi, AliPID::kProton );
        fTreeCascVarPosITSNSigmaPion   = fPIDResponse->NumberOfSigmasITS( pTrackXi, AliPID::kPion );
        fTreeCascVarPosITSNSigmaProton = fPIDResponse->NumberOfSigmasITS( pTrackXi, AliPID::kProton );
        fTreeCascVarBachITSNSigmaPion  = fPIDResponse->NumberOfSigmasITS( bachTrackXi, AliPID::kPion );
        fTreeCascVarBachITSNSigmaKaon  = fPIDResponse->NumberOfSigmasITS( bachTrackXi, AliPID::kKaon );
        
        //------------------------------------------------
        // Raw TPC dEdx + PIDForTracking information
        //------------------------------------------------
        
        //Step 1: Acquire TPC inner wall total momentum
        const AliExternalTrackParam *innerneg=nTrackXi->GetInnerParam();
        const AliExternalTrackParam *innerpos=pTrackXi->GetInnerParam();
        const AliExternalTrackParam *innerbach=bachTrackXi->GetInnerParam();
        fTreeCascVarPosInnerP = -1;
        fTreeCascVarNegInnerP = -1;
        fTreeCascVarBachInnerP = -1;
        if(innerpos)  { fTreeCascVarPosInnerP  = innerpos ->GetP(); }
        if(innerneg)  { fTreeCascVarNegInnerP  = innerneg ->GetP(); }
        if(innerbach) { fTreeCascVarBachInnerP = innerbach->GetP(); }
        
        //Step 2: Acquire TPC Signals
        fTreeCascVarPosdEdx = pTrackXi->GetTPCsignal();
        fTreeCascVarNegdEdx = nTrackXi->GetTPCsignal();
        fTreeCascVarBachdEdx = bachTrackXi->GetTPCsignal();
        
        //Step 3: Acquire PID For Tracking
        fTreeCascVarPosPIDForTracking = pTrackXi->GetPIDForTracking();
        fTreeCascVarNegPIDForTracking = nTrackXi->GetPIDForTracking();
        fTreeCascVarBachPIDForTracking = bachTrackXi->GetPIDForTracking();
        
        //------------------------------------------------
        // TPC Number of clusters info
        // --- modified to save the smallest number
        // --- of TPC clusters for the 3 tracks
        //------------------------------------------------
        
        lPosTPCClusters   = pTrackXi->GetTPCNcls();
        lNegTPCClusters   = nTrackXi->GetTPCNcls();
        lBachTPCClusters  = bachTrackXi->GetTPCNcls();
        
        // 1 - Poor quality related to TPCrefit
        ULong_t pStatus    = pTrackXi->GetStatus();
        ULong_t nStatus    = nTrackXi->GetStatus();
        ULong_t bachStatus = bachTrackXi->GetStatus();
        
        //fTreeCascVarkITSRefitBachelor = kTRUE;
        //fTreeCascVarkITSRefitNegative = kTRUE;
        //fTreeCascVarkITSRefitPositive = kTRUE;
        
        if ((pStatus&AliESDtrack::kTPCrefit)    == 0) {
            AliDebug(1, "Pb / V0 Pos. track has no TPCrefit ... continue!");
            continue;
        }
        if ((nStatus&AliESDtrack::kTPCrefit)    == 0) {
            AliDebug(1, "Pb / V0 Neg. track has no TPCrefit ... continue!");
            continue;
        }
        if ((bachStatus&AliESDtrack::kTPCrefit) == 0) {
            AliDebug(1, "Pb / Bach.   track has no TPCrefit ... continue!");
            continue;
        }
        
        //Get status flags
        fTreeCascVarPosTrackStatus = pTrackXi->GetStatus();
        fTreeCascVarNegTrackStatus = nTrackXi->GetStatus();
        fTreeCascVarBachTrackStatus = bachTrackXi->GetStatus();
        
        fTreeCascVarPosDCAz = GetDCAz(pTrackXi);
        fTreeCascVarNegDCAz = GetDCAz(nTrackXi);
        fTreeCascVarBachDCAz = GetDCAz(bachTrackXi);
        
        Float_t lPosChi2PerCluster = pTrackXi->GetTPCchi2() / ((Float_t) lPosTPCClusters);
        Float_t lNegChi2PerCluster = nTrackXi->GetTPCchi2() / ((Float_t) lNegTPCClusters);
        Float_t lBachChi2PerCluster = bachTrackXi->GetTPCchi2() / ((Float_t) lBachTPCClusters);
        
        Int_t leastnumberofclusters = 1000;
        Float_t lBiggestChi2PerCluster = -1;
        
        //Pick minimum
        if( lPosTPCClusters < leastnumberofclusters ) leastnumberofclusters = lPosTPCClusters;
        if( lNegTPCClusters < leastnumberofclusters ) leastnumberofclusters = lNegTPCClusters;
        if( lBachTPCClusters < leastnumberofclusters ) leastnumberofclusters = lBachTPCClusters;
        
        //Pick maximum
        if( lPosChi2PerCluster > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lPosChi2PerCluster;
        if( lNegChi2PerCluster > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lNegChi2PerCluster;
        if( lBachChi2PerCluster > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lBachChi2PerCluster;
        
        //Extra track quality: min track length
        Float_t lSmallestTrackLength = 1000;
        Float_t lPosTrackLength = -1;
        Float_t lNegTrackLength = -1;
        Float_t lBachTrackLength = -1;
        
        if (pTrackXi->GetInnerParam()) lPosTrackLength = pTrackXi->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (nTrackXi->GetInnerParam()) lNegTrackLength = nTrackXi->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (bachTrackXi->GetInnerParam()) lBachTrackLength = bachTrackXi->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        
        if ( lPosTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lPosTrackLength;
        if ( lNegTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lNegTrackLength;
        if ( lBachTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lBachTrackLength;
        
        fTreeCascVarMinTrackLength = lSmallestTrackLength;
        
        // 2 - Poor quality related to TPC clusters: lowest cut of 70 clusters
        if(lPosTPCClusters  < 70 && lSmallestTrackLength < 80) {
            AliDebug(1, "Pb / V0 Pos. track has less than 70 TPC clusters ... continue!");
            continue;
        }
        if(lNegTPCClusters  < 70 && lSmallestTrackLength < 80) {
            AliDebug(1, "Pb / V0 Neg. track has less than 70 TPC clusters ... continue!");
            continue;
        }
        if(lBachTPCClusters < 70 && lSmallestTrackLength < 80) {
            AliDebug(1, "Pb / Bach.   track has less than 70 TPC clusters ... continue!");
            continue;
        }
        
        lInvMassLambdaAsCascDghter	= xi->GetEffMass();
        // This value shouldn't change, whatever the working hyp. is : Xi-, Xi+, Omega-, Omega+
        lDcaV0DaughtersXi 		= xi->GetDcaV0Daughters();
        //lV0Chi2Xi 			= xi->GetChi2V0();
        
        
        lV0CosineOfPointingAngleXi 	= xi->GetV0CosineOfPointingAngle( lBestPrimaryVtxPos[0],
                                                                     lBestPrimaryVtxPos[1],
                                                                     lBestPrimaryVtxPos[2] );
        //Modification: V0 CosPA wrt to Cascade decay vertex
        lV0CosineOfPointingAngleXiSpecial 	= xi->GetV0CosineOfPointingAngle( lPosXi[0],
                                                                             lPosXi[1],
                                                                             lPosXi[2] );
        
        lDcaV0ToPrimVertexXi 		= xi->GetD( lBestPrimaryVtxPos[0],
                                               lBestPrimaryVtxPos[1],
                                               lBestPrimaryVtxPos[2] );
        
        lDcaBachToPrimVertexXi = TMath::Abs( bachTrackXi->GetD(	lBestPrimaryVtxPos[0],
                                                               lBestPrimaryVtxPos[1],
                                                               lMagneticField  ) );
        // Note : AliExternalTrackParam::GetD returns an algebraic value ...
        
        xi->GetXYZ( lPosV0Xi[0],  lPosV0Xi[1], lPosV0Xi[2] );
        lV0RadiusXi		= TMath::Sqrt( lPosV0Xi[0]*lPosV0Xi[0]  +  lPosV0Xi[1]*lPosV0Xi[1] );
        
        fTreeCascVarV0DecayX = lPosV0Xi[0];
        fTreeCascVarV0DecayY = lPosV0Xi[1];
        fTreeCascVarV0DecayZ = lPosV0Xi[2];
        
        //========================================================================================
        //Calculate V0 lifetime for adaptive decay radius cut
        //3D Distance travelled by the V0 in the cascade
        Float_t lV0DistanceTrav =  TMath::Sqrt(  TMath::Power( lPosV0Xi[0]-lPosXi[0] , 2)
                                               + TMath::Power( lPosV0Xi[1]-lPosXi[1] , 2)
                                               + TMath::Power( lPosV0Xi[2]-lPosXi[2] , 2) );
        
        //Total V0 momentum
        Float_t lV0TotMomentum = TMath::Sqrt(  TMath::Power( lNMom[0]+lPMom[0] , 2)
                                             + TMath::Power( lNMom[1]+lPMom[1] , 2)
                                             + TMath::Power( lNMom[2]+lPMom[2] , 2) );
        
        //V0 transverse momentum
        Float_t lV0Pt = TMath::Sqrt(  TMath::Power( lNMom[0]+lPMom[0] , 2)
                                    + TMath::Power( lNMom[1]+lPMom[1] , 2) );
        
        //Calculate V0 lifetime: mL/p
        if( TMath::Abs(lV0TotMomentum)>1e-5 ){
            fTreeCascVarV0Lifetime = 1.115683*lV0DistanceTrav / lV0TotMomentum;
        }else{
            fTreeCascVarV0Lifetime = -1;
        }
        //========================================================================================
        
        lDcaPosToPrimVertexXi 	= TMath::Abs( pTrackXi	->GetD(	lBestPrimaryVtxPos[0],
                                                               lBestPrimaryVtxPos[1],
                                                               lMagneticField  )     );
        
        lDcaNegToPrimVertexXi 	= TMath::Abs( nTrackXi	->GetD(	lBestPrimaryVtxPos[0],
                                                               lBestPrimaryVtxPos[1],
                                                               lMagneticField  )     );
        
        // - II.Step 4 : around effective masses (ESD)
        // ~ change mass hypotheses to cover all the possibilities :  Xi-/+, Omega -/+
        
        if( bachTrackXi->Charge() < 0 )	{
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , 3312);
            // Calculate the effective mass of the Xi- candidate.
            // pdg code 3312 = Xi-
            lInvMassXiMinus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , 3334);
            // Calculate the effective mass of the Xi- candidate.
            // pdg code 3334 = Omega-
            lInvMassOmegaMinus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , 3312); 	// Back to default hyp.
        }// end if negative bachelor
        
        
        if( bachTrackXi->Charge() >  0 ) {
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , -3312);
            // Calculate the effective mass of the Xi+ candidate.
            // pdg code -3312 = Xi+
            lInvMassXiPlus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , -3334);
            // Calculate the effective mass of the Xi+ candidate.
            // pdg code -3334  = Omega+
            lInvMassOmegaPlus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , -3312); 	// Back to "default" hyp.
        }// end if positive bachelor
        
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //Recalculate from scratch, please
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //WARNING: This will not be checked for correctness (charge-wise, etc)
        //         It will be up to the user to use the correct variable whenever needed!
        
        //+-+ Recalculate Xi Masses from scratch: will not change with lambda mass as
        //the perfect lambda mass is always assumed
        
        //+-+ Recalculate Lambda mass from scratch
        //Under Lambda hypothesis, the positive daughter is the proton, negative pion
        Double_t m1 = TDatabasePDG::Instance()->GetParticle(kProton)->Mass();
        Double_t m2 = TDatabasePDG::Instance()->GetParticle(kPiPlus)->Mass();
        Double_t e12   = m1*m1+lPMom[0]*lPMom[0]+lPMom[1]*lPMom[1]+lPMom[2]*lPMom[2];
        Double_t e22   = m2*m2+lNMom[0]*lNMom[0]+lNMom[1]*lNMom[1]+lNMom[2]*lNMom[2];
        fTreeCascVarV0MassLambda = TMath::Sqrt(TMath::Max(m1*m1+m2*m2
                                                          +2.*(TMath::Sqrt(e12*e22)-lPMom[0]*lNMom[0]-lPMom[1]*lNMom[1]-lPMom[2]*lNMom[2]),0.));
        
        //+-+ Recalculate AntiLambda mass from scratch
        //Under Lambda hypothesis, the positive daughter is the pion, negative antiproton
        m1 = TDatabasePDG::Instance()->GetParticle(kPiPlus)->Mass();
        m2 = TDatabasePDG::Instance()->GetParticle(kProton)->Mass();
        e12   = m1*m1+lPMom[0]*lPMom[0]+lPMom[1]*lPMom[1]+lPMom[2]*lPMom[2];
        e22   = m2*m2+lNMom[0]*lNMom[0]+lNMom[1]*lNMom[1]+lNMom[2]*lNMom[2];
        fTreeCascVarV0MassAntiLambda = TMath::Sqrt(TMath::Max(m1*m1+m2*m2
                                                              +2.*(TMath::Sqrt(e12*e22)-lPMom[0]*lNMom[0]-lPMom[1]*lNMom[1]-lPMom[2]*lNMom[2]),0.));
        
        
        
        // - II.Step 6 : extra info for QA (ESD)
        // miscellaneous pieces of info that may help regarding data quality assessment.
        //-------------
        xi->GetPxPyPz( lXiMomX, lXiMomY, lXiMomZ );
        lXiTransvMom  	= TMath::Sqrt( lXiMomX*lXiMomX   + lXiMomY*lXiMomY );
        lXiTotMom  	= TMath::Sqrt( lXiMomX*lXiMomX   + lXiMomY*lXiMomY   + lXiMomZ*lXiMomZ );
        
        xi->GetBPxPyPz(  lBachMomX,  lBachMomY,  lBachMomZ );
        //lBachTransvMom  = TMath::Sqrt( lBachMomX*lBachMomX   + lBachMomY*lBachMomY );
        //lBachTotMom  	= TMath::Sqrt( lBachMomX*lBachMomX   + lBachMomY*lBachMomY  +  lBachMomZ*lBachMomZ  );
        
        lChargeXi = xi->Charge();
        
        //lV0toXiCosineOfPointingAngle = xi->GetV0CosineOfPointingAngle( lPosXi[0], lPosXi[1], lPosXi[2] );
        
        lRapXi    = xi->RapXi();
        lRapOmega = xi->RapOmega();
        //lEta      = xi->Eta();
        //lTheta    = xi->Theta() *180.0/TMath::Pi();
        //lPhi      = xi->Phi()   *180.0/TMath::Pi();
        //lAlphaXi  = xi->AlphaXi();
        //lPtArmXi  = xi->PtArmXi();
        
        //----------------------------------------
        // Calculate Cascade DCA to PV, please
        //----------------------------------------
        
        Int_t lChargeCascade = fTreeCascVarCharge;
        
        //cascade properties to get started
        Double_t xyzCascade[3], pxpypzCascade[3], cvCascade[21];
        for(Int_t ii=0;ii<21;ii++) cvCascade[ii]=0.0; //something small
        
        xi->GetXYZcascade( xyzCascade[0],  xyzCascade[1], xyzCascade[2] );
        xi->GetPxPyPz( pxpypzCascade[0], pxpypzCascade[1], pxpypzCascade[2] );
        
        AliExternalTrackParam lCascTrajObject(xyzCascade,pxpypzCascade,cvCascade,lChargeCascade), *hCascTraj = &lCascTrajObject;
        
        Double_t lCascDCAtoPVxy = TMath::Abs(hCascTraj->GetD(lBestPrimaryVtxPos[0],
                                                             lBestPrimaryVtxPos[1],
                                                             lMagneticField) );
        Float_t dzcascade[2];
        hCascTraj->GetDZ(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2], lMagneticField, dzcascade );
        Double_t lCascDCAtoPVz = dzcascade[1];
        
        //assign TTree values
        fTreeCascVarCascDCAtoPVxy = lCascDCAtoPVxy;
        fTreeCascVarCascDCAtoPVz  = lCascDCAtoPVz;
        
        //----------------------------------------
        // Bump studies: perform propagation
        //----------------------------------------
        
        AliESDtrack *lBaryonTrack = 0x0;
        AliESDtrack *lBachelorTrack = 0x0;
        if ( lChargeXi == -1 ){
            lBaryonTrack = pTrackXi;
            lBachelorTrack = bachTrackXi;
        }
        if ( lChargeXi == +1 ){
            lBaryonTrack = nTrackXi;
            lBachelorTrack = bachTrackXi;
        }
        
        fTreeCascVarDCABachToBaryon = -100;
        
        Double_t bMag = lESDevent->GetMagneticField();
        Double_t xn, xp;
        
        //Care has to be taken here
        if ( lBaryonTrack && lBachelorTrack ){
            //Attempt zero: Calculate DCA between bachelor and baryon daughter
            fTreeCascVarDCABachToBaryon = lBaryonTrack->GetDCA(lBachelorTrack, bMag, xn, xp);
        }
        
        fTreeCascVarWrongCosPA = -1;
        if( bachTrackXi->Charge() < 0 )
        fTreeCascVarWrongCosPA = GetCosPA( bachTrackXi , pTrackXi, lESDevent );
        if( bachTrackXi->Charge() > 0 )
        fTreeCascVarWrongCosPA = GetCosPA( bachTrackXi , nTrackXi, lESDevent );
        
        
        //------------------------------------------------
        // Set Variables for adding to tree
        //------------------------------------------------
        
        fTreeCascVarMVPileupFlag = fMVPileupFlag;
        
        fTreeCascVarCharge	= lChargeXi;
        if (lChargeXi < 0 ){
            fTreeCascVarMassAsXi    = lInvMassXiMinus;
            fTreeCascVarMassAsOmega = lInvMassOmegaMinus;
        }
        if (lChargeXi > 0 ){
            fTreeCascVarMassAsXi    = lInvMassXiPlus;
            fTreeCascVarMassAsOmega = lInvMassOmegaPlus;
        }
        fTreeCascVarPt = lXiTransvMom;
        fTreeCascVarRapXi = lRapXi ;
        fTreeCascVarRapOmega = lRapOmega ;
        fTreeCascVarDCACascDaughters = lDcaXiDaughters;
        fTreeCascVarDCABachToPrimVtx = lDcaBachToPrimVertexXi;
        fTreeCascVarDCAV0Daughters = lDcaV0DaughtersXi;
        fTreeCascVarDCAV0ToPrimVtx = lDcaV0ToPrimVertexXi;
        fTreeCascVarDCAPosToPrimVtx = lDcaPosToPrimVertexXi;
        fTreeCascVarDCANegToPrimVtx = lDcaNegToPrimVertexXi;
        fTreeCascVarCascCosPointingAngle = lXiCosineOfPointingAngle;
        fTreeCascVarCascRadius = lXiRadius;
        fTreeCascVarV0Mass = lInvMassLambdaAsCascDghter;
        fTreeCascVarV0CosPointingAngle = lV0CosineOfPointingAngleXi;
        fTreeCascVarV0CosPointingAngleSpecial = lV0CosineOfPointingAngleXiSpecial;
        fTreeCascVarV0Radius = lV0RadiusXi;
        fTreeCascVarLeastNbrClusters = leastnumberofclusters;
        fTreeCascVarMaxChi2PerCluster = lBiggestChi2PerCluster;
        
        //Copy Multiplicity information
        fTreeCascVarCentrality = fCentrality;
        
        fTreeCascVarDistOverTotMom = TMath::Sqrt(
                                                 TMath::Power( lPosXi[0] - lBestPrimaryVtxPos[0] , 2) +
                                                 TMath::Power( lPosXi[1] - lBestPrimaryVtxPos[1] , 2) +
                                                 TMath::Power( lPosXi[2] - lBestPrimaryVtxPos[2] , 2)
                                                 );
        fTreeCascVarDistOverTotMom /= (lXiTotMom+1e-13);
        
        //Info for pileup studies
        if( fkDebugOOBPileup ) {
            fTreeCascVarBachTOFExpTDiff = bachTrackXi->GetTOFExpTDiff( bMag );
            fTreeCascVarNegTOFExpTDiff = nTrackXi->GetTOFExpTDiff( bMag );
            fTreeCascVarPosTOFExpTDiff = pTrackXi->GetTOFExpTDiff( bMag );
            fTreeCascVarBachTOFSignal = bachTrackXi->GetTOFsignal() * 1.e-3; // in ns
            fTreeCascVarNegTOFSignal = nTrackXi->GetTOFsignal() * 1.e-3; // in ns 
            fTreeCascVarPosTOFSignal = pTrackXi->GetTOFsignal() * 1.e-3; // in ns
            //Copy OOB pileup flag for this event
            fTreeCascVarOOBPileupFlag = fOOBPileupFlag;
            //Copy VZERO information for this event
            fTreeCascVarAmplitudeV0A = fAmplitudeV0A;
            fTreeCascVarAmplitudeV0C = fAmplitudeV0C;
            //Copy FMD information for this event
            fTreeCascVarNHitsFMDA = fNHitsFMDA;
            fTreeCascVarNHitsFMDC = fNHitsFMDC;
            //Copy IR information for this event
            fTreeCascVarClosestNonEmptyBC = fClosestNonEmptyBC;
        }
        
        if ( fkExtraCleanup ){
            //Meant to provide extra level of cleanup
            if( TMath::Abs(fTreeCascVarPosEta)>0.8 || TMath::Abs(fTreeCascVarNegEta)>0.8 || TMath::Abs(fTreeCascVarBachEta)>0.8 ) continue;
            if( TMath::Abs(fTreeCascVarRapXi)>0.5 && TMath::Abs(fTreeCascVarRapOmega)>0.5 ) continue;
            if ( fkPreselectDedx ){
                Bool_t lPassesPreFilterdEdx = kFALSE;
                //XiMinus Pre-selection
                if( fTreeCascVarMassAsXi<1.32+0.250&&fTreeCascVarMassAsXi>1.32-0.250 && TMath::Abs(fTreeCascVarPosNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaPion) < 5.0 && fTreeCascVarCharge == -1 ) lPassesPreFilterdEdx = kTRUE;
                if( fTreeCascVarMassAsXi<1.32+0.250&&fTreeCascVarMassAsXi>1.32-0.250 && TMath::Abs(fTreeCascVarPosNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaPion) < 5.0 && fTreeCascVarCharge == +1 ) lPassesPreFilterdEdx = kTRUE;
                if(fTreeCascVarMassAsOmega<1.68+0.250&&fTreeCascVarMassAsOmega>1.68-0.250 && TMath::Abs(fTreeCascVarPosNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaKaon) < 5.0 && fTreeCascVarCharge == -1  ) lPassesPreFilterdEdx = kTRUE;
                if(fTreeCascVarMassAsOmega<1.68+0.250&&fTreeCascVarMassAsOmega>1.68-0.250 && TMath::Abs(fTreeCascVarPosNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaKaon) < 5.0 && fTreeCascVarCharge == +1) lPassesPreFilterdEdx = kTRUE;
                if( !lPassesPreFilterdEdx ) continue;
            }
        }
        
        //All vars not specified here: specified elsewhere!
        
        //------------------------------------------------
        // Fill Tree!
        //------------------------------------------------
        
        // The conditional is meant to decrease excessive
        // memory usage! Be careful when loosening the
        // cut!
        
        //Xi    Mass window: 150MeV wide
        //Omega mass window: 150MeV wide
        
        //Random denial
        Bool_t lKeepCascade = kTRUE;
        if(fkDownScaleCascade && ( fRand->Uniform() > fDownScaleFactorCascade )) lKeepCascade = kFALSE;
        
        //Lowest pT cutoff (this is all background anyways)
        if( fTreeCascVarPt < fMinPtToSave ) lKeepCascade = kFALSE;
        if( fTreeCascVarPt > fMaxPtToSave ) lKeepCascade = kFALSE;
        
        if( fkSelectCharge !=0 && fkSelectCharge !=  fTreeCascVarCharge ) lKeepCascade = kFALSE;
        
        if( fkSaveCascadeTree && lKeepCascade &&
           (
            (//START XI SELECTIONS
             (fTreeCascVarMassAsXi<1.32+0.075&&fTreeCascVarMassAsXi>1.32-0.075)
             )//end Xi Selections
            ||
            (//START OMEGA SELECTIONS
             (fTreeCascVarMassAsOmega<1.68+0.075&&fTreeCascVarMassAsOmega>1.68-0.075)
             )//end Xi Selections
            )
           )
        {
            fTreeCascade->Fill();
        }
        //------------------------------------------------
        // Fill tree over.
        //------------------------------------------------
        
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //Step 1: Sweep members of the output object TList and fill all of them as appropriate
        Int_t lNumberOfConfigurationsCascade = fListCascade->GetEntries();
        //AliWarning(Form("[Cascade Analyses] Processing different configurations (%i detected)",lNumberOfConfigurationsCascade));
        TH3F *histoout         = 0x0;
        AliCascadeResult *lCascadeResult = 0x0;
        for(Int_t lcfg=0; lcfg<lNumberOfConfigurationsCascade; lcfg++){
            lCascadeResult = (AliCascadeResult*) fListCascade->At(lcfg);
            histoout  = lCascadeResult->GetHistogram();
            
            Float_t lMass = 0;
            Float_t lV0Mass = 0;
            Float_t lRap  = 0;
            Float_t lPDGMass = -1;
            Float_t lNegdEdx = 100;
            Float_t lPosdEdx = 100;
            Float_t lBachdEdx = 100;
            Float_t lNegTOFsigma = 100;
            Float_t lPosTOFsigma = 100;
            Float_t lBachTOFsigma = 100;
            Short_t  lCharge = -2;
            Int_t lChargePos =  1;
            Int_t lChargeNeg = -1;
            Float_t lprpx, lprpy, lprpz, lpipx, lpipy, lpipz;
            lpipx = fTreeCascVarBachPx;
            lpipy = fTreeCascVarBachPy;
            lpipz = fTreeCascVarBachPz;
            
            //For parametric V0 Mass selection
            Float_t lExpV0Mass =
            fLambdaMassMean[0]+
            fLambdaMassMean[1]*TMath::Exp(fLambdaMassMean[2]*lV0Pt)+
            fLambdaMassMean[3]*TMath::Exp(fLambdaMassMean[4]*lV0Pt);
            
            Float_t lExpV0Sigma =
            fLambdaMassSigma[0]+fLambdaMassSigma[1]*lV0Pt+
            fLambdaMassSigma[2]*TMath::Exp(fLambdaMassSigma[3]*lV0Pt);
            
            //========================================================================
            //For 2.76TeV-like parametric V0 CosPA
            Float_t l276TeVV0CosPA = 0.998;
            Float_t pThr=1.5;
            if (lV0TotMomentum<pThr) {
                //Below the threshold "pThr", try a momentum dependent cos(PA) cut
                const Double_t bend=0.03; // approximate Xi bending angle
                const Double_t qt=0.211;  // max Lambda pT in Omega decay
                const Double_t cpaThr=TMath::Cos(TMath::ATan(qt/pThr) + bend);
                Double_t
                cpaCut=(0.998/cpaThr)*TMath::Cos(TMath::ATan(qt/lV0TotMomentum) + bend);
                l276TeVV0CosPA = cpaCut;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable Cascade CosPA
            Float_t lCascCosPACut = lCascadeResult -> GetCutCascCosPA();
            Float_t lVarCascCosPApar[5];
            lVarCascCosPApar[0] = lCascadeResult->GetCutVarCascCosPAExp0Const();
            lVarCascCosPApar[1] = lCascadeResult->GetCutVarCascCosPAExp0Slope();
            lVarCascCosPApar[2] = lCascadeResult->GetCutVarCascCosPAExp1Const();
            lVarCascCosPApar[3] = lCascadeResult->GetCutVarCascCosPAExp1Slope();
            lVarCascCosPApar[4] = lCascadeResult->GetCutVarCascCosPAConst();
            Float_t lVarCascCosPA = TMath::Cos(
                                               lVarCascCosPApar[0]*TMath::Exp(lVarCascCosPApar[1]*fTreeCascVarPt) +
                                               lVarCascCosPApar[2]*TMath::Exp(lVarCascCosPApar[3]*fTreeCascVarPt) +
                                               lVarCascCosPApar[4]);
            if( lCascadeResult->GetCutUseVarCascCosPA() ){
                //Only use if tighter than the non-variable cut
                if( lVarCascCosPA > lCascCosPACut ) lCascCosPACut = lVarCascCosPA;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable V0 CosPA
            Float_t lV0CosPACut = lCascadeResult -> GetCutV0CosPA();
            Float_t lVarV0CosPApar[5];
            lVarV0CosPApar[0] = lCascadeResult->GetCutVarV0CosPAExp0Const();
            lVarV0CosPApar[1] = lCascadeResult->GetCutVarV0CosPAExp0Slope();
            lVarV0CosPApar[2] = lCascadeResult->GetCutVarV0CosPAExp1Const();
            lVarV0CosPApar[3] = lCascadeResult->GetCutVarV0CosPAExp1Slope();
            lVarV0CosPApar[4] = lCascadeResult->GetCutVarV0CosPAConst();
            Float_t lVarV0CosPA = TMath::Cos(
                                             lVarV0CosPApar[0]*TMath::Exp(lVarV0CosPApar[1]*fTreeCascVarPt) +
                                             lVarV0CosPApar[2]*TMath::Exp(lVarV0CosPApar[3]*fTreeCascVarPt) +
                                             lVarV0CosPApar[4]);
            if( lCascadeResult->GetCutUseVarV0CosPA() ){
                //Only use if tighter than the non-variable cut
                if( lVarV0CosPA > lV0CosPACut ) lV0CosPACut = lVarV0CosPA;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable BB CosPA
            Float_t lBBCosPACut = lCascadeResult -> GetCutBachBaryonCosPA();
            Float_t lVarBBCosPApar[5];
            lVarBBCosPApar[0] = lCascadeResult->GetCutVarBBCosPAExp0Const();
            lVarBBCosPApar[1] = lCascadeResult->GetCutVarBBCosPAExp0Slope();
            lVarBBCosPApar[2] = lCascadeResult->GetCutVarBBCosPAExp1Const();
            lVarBBCosPApar[3] = lCascadeResult->GetCutVarBBCosPAExp1Slope();
            lVarBBCosPApar[4] = lCascadeResult->GetCutVarBBCosPAConst();
            Float_t lVarBBCosPA = TMath::Cos(
                                             lVarBBCosPApar[0]*TMath::Exp(lVarBBCosPApar[1]*fTreeCascVarPt) +
                                             lVarBBCosPApar[2]*TMath::Exp(lVarBBCosPApar[3]*fTreeCascVarPt) +
                                             lVarBBCosPApar[4]);
            if( lCascadeResult->GetCutUseVarBBCosPA() ){
                //Only use if looser than the non-variable cut (WARNING: BEWARE INVERSE LOGIC)
                if( lVarBBCosPA > lBBCosPACut ) lBBCosPACut = lVarBBCosPA;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable DCA Casc Dau
            Float_t lDCACascDauCut = lCascadeResult -> GetCutDCACascDaughters();
            Float_t lVarDCACascDaupar[5];
            lVarDCACascDaupar[0] = lCascadeResult->GetCutVarDCACascDauExp0Const();
            lVarDCACascDaupar[1] = lCascadeResult->GetCutVarDCACascDauExp0Slope();
            lVarDCACascDaupar[2] = lCascadeResult->GetCutVarDCACascDauExp1Const();
            lVarDCACascDaupar[3] = lCascadeResult->GetCutVarDCACascDauExp1Slope();
            lVarDCACascDaupar[4] = lCascadeResult->GetCutVarDCACascDauConst();
            Float_t lVarDCACascDau = lVarDCACascDaupar[0]*TMath::Exp(lVarDCACascDaupar[1]*fTreeCascVarPt) +
            lVarDCACascDaupar[2]*TMath::Exp(lVarDCACascDaupar[3]*fTreeCascVarPt) +
            lVarDCACascDaupar[4];
            if( lCascadeResult->GetCutUseVarDCACascDau() ){
                //Loosest: default cut, parametric can go tighter
                if( lVarDCACascDau < lDCACascDauCut ) lDCACascDauCut = lVarDCACascDau;
            }
            //========================================================================
            
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kXiMinus     ){
                lCharge  = -1;
                if ( lCascadeResult->GetSwapBachelorCharge() ) lCharge *= -1;
                lMass    = fTreeCascVarMassAsXi;
                lV0Mass  = fTreeCascVarV0MassLambda;
                lRap     = fTreeCascVarRapXi;
                lPDGMass = 1.32171;
                lNegdEdx = fTreeCascVarNegNSigmaPion;
                lPosdEdx = fTreeCascVarPosNSigmaProton;
                lBachdEdx= fTreeCascVarBachNSigmaPion;
                lNegTOFsigma = fTreeCascVarNegTOFNSigmaPion;
                lPosTOFsigma = fTreeCascVarPosTOFNSigmaProton;
                lBachTOFsigma = fTreeCascVarBachTOFNSigmaPion;
                lprpx = fTreeCascVarPosPx;
                lprpy = fTreeCascVarPosPy;
                lprpz = fTreeCascVarPosPz;
            }
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kXiPlus      ){
                lCharge  = +1;
                if ( lCascadeResult->GetSwapBachelorCharge() ) lCharge *= -1;
                lMass    = fTreeCascVarMassAsXi;
                lV0Mass  = fTreeCascVarV0MassAntiLambda;
                lRap     = fTreeCascVarRapXi;
                lPDGMass = 1.32171;
                lNegdEdx = fTreeCascVarNegNSigmaProton;
                lPosdEdx = fTreeCascVarPosNSigmaPion;
                lBachdEdx= fTreeCascVarBachNSigmaPion;
                lNegTOFsigma = fTreeCascVarNegTOFNSigmaProton;
                lPosTOFsigma = fTreeCascVarPosTOFNSigmaPion;
                lBachTOFsigma = fTreeCascVarBachTOFNSigmaPion;
                lprpx = fTreeCascVarNegPx;
                lprpy = fTreeCascVarNegPy;
                lprpz = fTreeCascVarNegPz;
            }
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kOmegaMinus     ){
                lCharge  = -1;
                if ( lCascadeResult->GetSwapBachelorCharge() ) lCharge *= -1;
                lMass    = fTreeCascVarMassAsOmega;
                lV0Mass  = fTreeCascVarV0MassLambda;
                lRap     = fTreeCascVarRapOmega;
                lPDGMass = 1.67245;
                lNegdEdx = fTreeCascVarNegNSigmaPion;
                lPosdEdx = fTreeCascVarPosNSigmaProton;
                lBachdEdx= fTreeCascVarBachNSigmaKaon;
                lNegTOFsigma = fTreeCascVarNegTOFNSigmaPion;
                lPosTOFsigma = fTreeCascVarPosTOFNSigmaProton;
                lBachTOFsigma = fTreeCascVarBachTOFNSigmaKaon;
                lprpx = fTreeCascVarPosPx;
                lprpy = fTreeCascVarPosPy;
                lprpz = fTreeCascVarPosPz;
            }
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kOmegaPlus      ){
                lCharge  = +1;
                if ( lCascadeResult->GetSwapBachelorCharge() ) lCharge *= -1;
                lMass    = fTreeCascVarMassAsOmega;
                lV0Mass  = fTreeCascVarV0MassAntiLambda;
                lRap     = fTreeCascVarRapOmega;
                lPDGMass = 1.67245;
                lNegdEdx = fTreeCascVarNegNSigmaProton;
                lPosdEdx = fTreeCascVarPosNSigmaPion;
                lBachdEdx= fTreeCascVarBachNSigmaKaon;
                lNegTOFsigma = fTreeCascVarNegTOFNSigmaProton;
                lPosTOFsigma = fTreeCascVarPosTOFNSigmaPion;
                lBachTOFsigma = fTreeCascVarBachTOFNSigmaKaon;
                lprpx = fTreeCascVarNegPx;
                lprpy = fTreeCascVarNegPy;
                lprpz = fTreeCascVarNegPz;
            }
            
            if (lCascadeResult->GetCutUseTOFUnchecked() == kFALSE ){
                //Always-pass values
                lNegTOFsigma = 0;
                lPosTOFsigma = 0;
                lBachTOFsigma = 0;
            }
            
            if (
                //Check 1: Charge consistent with expectations
                fTreeCascVarCharge == lCharge &&
                
                //Check 2: Basic Acceptance cuts
                lCascadeResult->GetCutMinEtaTracks() < fTreeCascVarPosEta && fTreeCascVarPosEta < lCascadeResult->GetCutMaxEtaTracks() &&
                lCascadeResult->GetCutMinEtaTracks() < fTreeCascVarNegEta && fTreeCascVarNegEta < lCascadeResult->GetCutMaxEtaTracks() &&
                lCascadeResult->GetCutMinEtaTracks() < fTreeCascVarBachEta && fTreeCascVarBachEta < lCascadeResult->GetCutMaxEtaTracks() &&
                lRap > lCascadeResult->GetCutMinRapidity() &&
                lRap < lCascadeResult->GetCutMaxRapidity() &&
                
                //Check 3: Topological Variables
                // - V0 Selections
                fTreeCascVarDCANegToPrimVtx > lCascadeResult->GetCutDCANegToPV() &&
                fTreeCascVarDCAPosToPrimVtx > lCascadeResult->GetCutDCAPosToPV() &&
                fTreeCascVarDCAV0Daughters < lCascadeResult->GetCutDCAV0Daughters() &&
                fTreeCascVarV0CosPointingAngle > lV0CosPACut &&
                fTreeCascVarV0Radius > lCascadeResult->GetCutV0Radius() &&
                // - Cascade Selections
                fTreeCascVarDCAV0ToPrimVtx > lCascadeResult->GetCutDCAV0ToPV() &&
                TMath::Abs(lV0Mass-1.116) < lCascadeResult->GetCutV0Mass() &&
                fTreeCascVarDCABachToPrimVtx > lCascadeResult->GetCutDCABachToPV() &&
                fTreeCascVarDCACascDaughters < lDCACascDauCut &&
                fTreeCascVarCascCosPointingAngle > lCascCosPACut &&
                fTreeCascVarCascRadius > lCascadeResult->GetCutCascRadius() &&
                
                // - Implementation of a parametric V0 Mass cut if requested
                (
                 ( lCascadeResult->GetCutV0MassSigma() > 50 ) || //anything goes
                 (TMath::Abs( (lV0Mass-lExpV0Mass) / lExpV0Sigma ) < lCascadeResult->GetCutV0MassSigma() )
                 ) &&
                
                // - Miscellaneous
                fTreeCascVarDistOverTotMom*lPDGMass < lCascadeResult->GetCutProperLifetime() &&
                fTreeCascVarLeastNbrClusters > lCascadeResult->GetCutLeastNumberOfClusters() &&
                
                //Check 4: TPC dEdx selections
                TMath::Abs(lNegdEdx )<lCascadeResult->GetCutTPCdEdx() &&
                TMath::Abs(lPosdEdx )<lCascadeResult->GetCutTPCdEdx() &&
                TMath::Abs(lBachdEdx)<lCascadeResult->GetCutTPCdEdx() &&
                
                //Check 4bis: TOF selections (experimental)
                //WARNING: if lCascadeResult->GetCutUseTOFUnchecked is false, the TOFsigmas will be zero: will always pass
                TMath::Abs(lNegTOFsigma )< 4 &&
                TMath::Abs(lPosTOFsigma )< 4 &&
                TMath::Abs(lBachTOFsigma)< 4 &&
                
                //Check 5: Xi rejection for Omega analysis
                ( ( lCascadeResult->GetMassHypothesis() != AliCascadeResult::kOmegaMinus && lCascadeResult->GetMassHypothesis() != AliCascadeResult::kOmegaPlus  ) || ( TMath::Abs( fTreeCascVarMassAsXi - 1.32171 ) > lCascadeResult->GetCutXiRejection() ) ) &&
                
                //Check 6: Experimental DCA Bachelor to Baryon cut
                ( fTreeCascVarDCABachToBaryon > lCascadeResult->GetCutDCABachToBaryon() ) &&
                
                //Check 7: Experimental Bach Baryon CosPA
                ( fTreeCascVarWrongCosPA < lBBCosPACut  ) &&
                
                //Check 8: Min/Max V0 Lifetime cut
                ( ( fTreeCascVarV0Lifetime > lCascadeResult->GetCutMinV0Lifetime() ) &&
                 ( fTreeCascVarV0Lifetime < lCascadeResult->GetCutMaxV0Lifetime() ||
                  lCascadeResult->GetCutMaxV0Lifetime() > 1e+3 ) ) &&
                
                //Check 9: kITSrefit track selection if requested
                (
                 ( (fTreeCascVarPosTrackStatus & AliESDtrack::kITSrefit) &&
                  (fTreeCascVarNegTrackStatus & AliESDtrack::kITSrefit) &&
                  (fTreeCascVarBachTrackStatus & AliESDtrack::kITSrefit)
                  )
                 ||
                 !lCascadeResult->GetCutUseITSRefitTracks()
                 ) &&
                
                //Check 10: Max Chi2/Clusters if not absurd
                ( lCascadeResult->GetCutMaxChi2PerCluster()>1e+3 ||
                 fTreeCascVarMaxChi2PerCluster < lCascadeResult->GetCutMaxChi2PerCluster()
                 )&&
                
                //Check 11: Min Track Length if positive
                ( lCascadeResult->GetCutMinTrackLength()<0 || //this is a bit paranoid...
                 fTreeCascVarMinTrackLength > lCascadeResult->GetCutMinTrackLength()
                 )&&
                
                //Check 12: Check if special V0 CosPA cut used
                //either don't use the cut at all, or make sure it's above threshold
                ( lCascadeResult->GetCutUse276TeVV0CosPA()==kFALSE ||
                 fTreeCascVarV0CosPointingAngle>l276TeVV0CosPA
                 )&&
                
                //Check 13: 3D Cascade DCA to PV
                ( lCascadeResult->GetCutDCACascadeToPV() > 999 ||
                 (TMath::Sqrt(fTreeCascVarCascDCAtoPVz*fTreeCascVarCascDCAtoPVz + fTreeCascVarCascDCAtoPVxy*fTreeCascVarCascDCAtoPVxy)<lCascadeResult->GetCutDCACascadeToPV() )
                 )&&
                
                //Check 14a: Negative track DCA to PV, weighted
                ( lCascadeResult->GetCutDCANegToPVWeighted() < 0 ||
                 (fTreeCascVarDCANegToPrimVtx/TMath::Sqrt(fTreeCascVarNegDCAPVSigmaX2*fTreeCascVarNegDCAPVSigmaX2 + fTreeCascVarNegDCAPVSigmaY2*fTreeCascVarNegDCAPVSigmaY2+1e-6)>lCascadeResult->GetCutDCANegToPVWeighted() )
                 )&&
                //Check 14a: Negative track DCA to PV, weighted
                ( lCascadeResult->GetCutDCAPosToPVWeighted() < 0 ||
                 (fTreeCascVarDCAPosToPrimVtx/TMath::Sqrt(fTreeCascVarPosDCAPVSigmaX2*fTreeCascVarPosDCAPVSigmaX2 + fTreeCascVarPosDCAPVSigmaY2*fTreeCascVarPosDCAPVSigmaY2+1e-6)>lCascadeResult->GetCutDCAPosToPVWeighted() )
                 )&&
                //Check 14a: Negative track DCA to PV, weighted
                ( lCascadeResult->GetCutDCABachToPVWeighted() < 0 ||
                 (fTreeCascVarDCABachToPrimVtx/TMath::Sqrt(fTreeCascVarBachDCAPVSigmaX2*fTreeCascVarBachDCAPVSigmaX2 + fTreeCascVarBachDCAPVSigmaY2*fTreeCascVarBachDCAPVSigmaY2+1e-6)>lCascadeResult->GetCutDCABachToPVWeighted() )
                 )
                )
            {
                //This satisfies all my conditionals! Fill histogram
                histoout -> Fill ( fCentrality, fTreeCascVarPt, lMass );
            }
        }
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // End Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
    }// end of the Cascade loop (ESD or AOD)
    
    // Post output data.
    PostData(1, fListHist    );
    PostData(2, fListV0      );
    PostData(3, fListCascade );
    if( fkSaveEventTree   ) PostData(4, fTreeEvent   );
    if( fkSaveV0Tree      ) PostData(5, fTreeV0      );
    if( fkSaveCascadeTree ) PostData(6, fTreeCascade );
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::Terminate(Option_t *)
{
    // Draw result to the screen
    // Called once at the end of the query
    
    TList *cRetrievedList = 0x0;
    cRetrievedList = (TList*)GetOutputData(1);
    if(!cRetrievedList) {
        Printf("ERROR - AliAnalysisTaskStrangenessVsMultiplicityRun2 : ouput data container list not available\n");
        return;
    }
    
    fHistEventCounter = dynamic_cast<TH1D*> (  cRetrievedList->FindObject("fHistEventCounter")  );
    if (!fHistEventCounter) {
        Printf("ERROR - AliAnalysisTaskStrangenessVsMultiplicityRun2 : fHistEventCounter not available");
        return;
    }
    
    TCanvas *canCheck = new TCanvas("AliAnalysisTaskStrangenessVsMultiplicityRun2","V0 Multiplicity",10,10,510,510);
    canCheck->cd(1)->SetLogy();
    
    fHistEventCounter->SetMarkerStyle(22);
    fHistEventCounter->DrawCopy("E");
}

//________________________________________________________________________
Double_t AliAnalysisTaskStrangenessVsMultiplicityRun2::MyRapidity(Double_t rE, Double_t rPz) const
{
    // Local calculation for rapidity
    Double_t ReturnValue = -100;
    if( (rE-rPz+1.e-13) != 0 && (rE+rPz) != 0 ) {
        ReturnValue =  0.5*TMath::Log((rE+rPz)/(rE-rPz+1.e-13));
    }
    return ReturnValue;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddConfiguration( AliV0Result *lV0Result )
{
    if (!fListV0){
        Printf("fListV0 does not exist. Creating...");
        fListV0 = new TList();
        fListV0->SetOwner();
        
    }
    fListV0->Add(lV0Result);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddConfiguration( AliCascadeResult *lCascadeResult )
{
    if (!fListCascade){
        Printf("fListCascade does not exist. Creating...");
        fListCascade = new TList();
        fListCascade->SetOwner();
        
    }
    fListCascade->Add(lCascadeResult);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::SetupStandardVertexing()
//Meant to store standard re-vertexing configuration
{
    //Tell the task to re-run vertexers
    SetRunVertexers(kTRUE);
    SetDoV0Refit(kTRUE);
    
    //V0-Related topological selections
    SetV0VertexerDCAFirstToPV(0.05);
    SetV0VertexerDCASecondtoPV(0.05);
    SetV0VertexerDCAV0Daughters(1.20);
    SetV0VertexerCosinePA(0.98);
    SetV0VertexerMinRadius(0.9);
    SetV0VertexerMaxRadius(200);
    
    //Cascade-Related topological selections
    SetCascVertexerMinV0ImpactParameter(0.05);
    SetCascVertexerV0MassWindow(0.006);
    SetCascVertexerDCABachToPV(0.02);
    SetCascVertexerDCACascadeDaughters(1.2);
    SetCascVertexerCascadeMinRadius(.8);
    SetCascVertexerCascadeCosinePA(.98);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::SetupLooseVertexing()
//Meant to store standard re-vertexing configuration
{
    //Tell the task to re-run vertexers
    SetRunVertexers(kTRUE);
    SetDoV0Refit(kTRUE);
    
    //V0-Related topological selections
    SetV0VertexerDCAFirstToPV(0.1);
    SetV0VertexerDCASecondtoPV(0.1);
    SetV0VertexerDCAV0Daughters(1.40);
    SetV0VertexerCosinePA(0.95);
    SetV0VertexerMinRadius(0.9);
    SetV0VertexerMaxRadius(200);
    
    //Cascade-Related topological selections
    SetCascVertexerMinV0ImpactParameter(0.05);
    SetCascVertexerV0MassWindow(0.006);
    SetCascVertexerDCABachToPV(0.02);
    SetCascVertexerDCACascadeDaughters(1.4);
    SetCascVertexerCascadeMinRadius(.5);
    SetCascVertexerCascadeCosinePA(.95);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddTopologicalQAV0(Int_t lRecNumberOfSteps)
//Add all configurations to do QA of topological variables for the V0 analysis
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0,
        2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 15};
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    Double_t lPtbinlimitsCascade[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0,
        2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 14, 17, 20};
    Long_t lPtbinnumbCascade = sizeof(lPtbinlimitsCascade)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 10};
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleName[] = {"K0Short", "Lambda",  "AntiLambda"};
    
    //STEP 3: Creation of output objects
    
    //Map to mass hypothesis
    AliV0Result::EMassHypo lMassHypoV0[3];
    lMassHypoV0[0] = AliV0Result::kK0Short;
    lMassHypoV0[1] = AliV0Result::kLambda;
    lMassHypoV0[2] = AliV0Result::kAntiLambda;
    
    Float_t lLifetimeCut[3];
    lLifetimeCut[0] = 20.0;
    lLifetimeCut[1] = 30.0;
    lLifetimeCut[2] = 30.0;
    
    Float_t lMass[3];
    lMass[0] = 0.497;
    lMass[1] = 1.116;
    lMass[2] = 1.116;
    
    Float_t lMWindow[3];
    lMWindow[0] = 0.075;
    lMWindow[1] = 0.050;
    lMWindow[2] = 0.050;
    
    //Array of results
    AliV0Result *lV0Result[5000];
    Long_t lNV0 = 0;
    
    //Central results: Stored in indices 0, 1, 2 (careful!)
    for(Int_t i = 0 ; i < 3 ; i ++){
        //Central result, customized binning: the one to use, usually
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central",lParticleName[i].Data() ),lMassHypoV0[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits, 100,lMass[i]-lMWindow[i],lMass[i]+lMWindow[i]);
        //if ( i>0 ) not neeed for real data
        //    lV0Result[lNV0]->InitializeFeeddownMatrix( lPtbinnumb, lPtbinlimits, lPtbinnumbCascade, lPtbinlimitsCascade, lCentbinnumb, lCentbinlimits );
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( 0.05 ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( 0.05 ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( 1.2 ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( 0.98 ) ;
        lV0Result[lNV0]->SetCutV0Radius              ( 0.9 ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lLifetimeCut[i] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( 70 ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( 0.8 ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( 4 ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Will now proceed to sweep individual variables
    //Number of steps used for the variable sweeps
    const Int_t lNumberOfSteps = lRecNumberOfSteps;
    
    //________________________________________________________
    // Variable 1: DCA Neg to PV
    Float_t lMaxDCANegToPV = 20.00;
    
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCANegToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCANegToPV / ((Float_t) lNumberOfSteps) ;
            lV0Result[lNV0] -> SetCutDCANegToPV ( lThisCut );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 2: DCA Pos to PV
    Float_t lMaxDCAPosToPV = 20.00;
    
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAPosToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAPosToPV / ((Float_t) lNumberOfSteps) ;
            lV0Result[lNV0] -> SetCutDCAPosToPV ( lThisCut );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 3: DCA V0 Daughters
    Float_t lMaxDCAV0Daughters = 1.20;
    
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAV0DaughtersSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAV0Daughters / ((Float_t) lNumberOfSteps) ;
            lV0Result[lNV0] -> SetCutDCAV0Daughters ( lThisCut );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 4: V0 CosPA
    Float_t lMinV0CosPA = 0.98;
    Float_t lMaxV0CosPA = 1.00;
    Double_t lV0CosPAVals[lNumberOfSteps];
    Double_t lMinV0PA = 0.0;
    Double_t lMaxV0PA = TMath::ACos(lMinV0CosPA);
    Double_t lDeltaV0PA = lMaxV0PA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lV0CosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaV0PA );
    }
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0CosPASweep",icut) );
            //Add result to pool
            lV0Result[lNV0] -> SetCutV0CosPA ( lV0CosPAVals[icut] );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 5: V0 Radius
    Float_t lMinV0Radius = 2.0;
    Float_t lMaxV0Radius = 20.00;
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0RadiusSweep",icut) );
            //Add result to pool
            Float_t lThisCut = lMinV0Radius + (lMaxV0Radius-lMinV0Radius)*(((Float_t)icut)+1)/((Float_t)lNumberOfSteps);
            lV0Result[lNV0] -> SetCutV0Radius ( lThisCut );
            lNV0++;
        }
    }
    for (Int_t iconf = 0; iconf<lNV0; iconf++)
    AddConfiguration(lV0Result[iconf]);
    
    cout<<"Added "<<lNV0<<" V0 configurations to output."<<endl;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddTopologicalQACascade(Int_t lRecNumberOfSteps)
//Add all configurations to do QA of topological variables for the V0 analysis
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] = {0.4, 0.5, 0.6,
        0.7,0.8,.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,
        2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,3.0,3.2,3.4,3.6,3.8,4.0,4.2,
        4.4,4.6,4.8,5.0,5.5,6.0,6.5,7.0,8.0,9.0,10.,11.,12.};
    //Double_t lPtbinlimits[] = {0.2,0.3, 0.4, 0.5, 0.6,
    //    0.7,0.8,.9,1.0,1.2, 1.4, 1.6, 1.8 ,2.0,
    //    2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0,
    //    4.4,4.8,5.0,6.0,7.0,8.0,9.0,10.,11.,12.};
    
    //Double_t lPtbinlimits[] = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4, 2.8, 3.2,
    //3.6, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.5, 8.5, 10, 12};
    
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 10}; //optimize in 0-10%
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    //Just a counter and one array, please. Nothing else needed
    AliCascadeResult *lCascadeResult[5000];
    Long_t lN = 0;
    
    //Map to mass hypothesis
    AliCascadeResult::EMassHypo lMassHypo[4];
    lMassHypo[0] = AliCascadeResult::kXiMinus;
    lMassHypo[1] = AliCascadeResult::kXiPlus;
    lMassHypo[2] = AliCascadeResult::kOmegaMinus;
    lMassHypo[3] = AliCascadeResult::kOmegaPlus;
    
    Float_t lLifetimeCut[4];
    lLifetimeCut[0] = 15.0;
    lLifetimeCut[1] = 15.0;
    lLifetimeCut[2] = 12.0;
    lLifetimeCut[3] = 12.0;
    
    Float_t lMass[4];
    lMass[0] = 1.322;
    lMass[1] = 1.322;
    lMass[2] = 1.672;
    lMass[3] = 1.672;
    
    TString lParticleName[] = {"XiMinus", "XiPlus",  "OmegaMinus", "OmegaPlus"};
    
    //Number of steps used for the variable sweeps
    const Int_t lNumberOfSteps = lRecNumberOfSteps;
    
    //Central results: Stored in indices 0, 1, 2, 3 (careful!)
    for(Int_t i = 0 ; i < 4 ; i ++){
        //Central result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_VertexerLevel",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits,100,lMass[i]-0.050,lMass[i]+0.050);
        
        //Default cuts: use vertexer level ones
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        (  1. ) ;
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.95 ) ; //+variable
        lCascadeResult[lN]->SetCutVarV0CosPA            (TMath::Exp(10.853),
                                                         -25.0322,
                                                         TMath::Exp(-0.843948),
                                                         -0.890794,
                                                         0.057553);
        lCascadeResult[lN]->SetCutV0Radius              (  3 ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( 0.1 ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( 0.006 ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( 0.1 ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( 1.0) ;
        lCascadeResult[lN]->SetCutCascRadius            ( 1.2 ) ;
        if(i==2||i==3)
        lCascadeResult[lN]->SetCutCascRadius            ( 1.0 ) ; //omega case
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.95 ) ; //+variable
        lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(4.86664),
                                                         -10.786,
                                                         TMath::Exp(-1.33411),
                                                         -0.729825,
                                                         0.0695724);
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( lLifetimeCut[i] ) ;
        lCascadeResult[lN]->SetCutLeastNumberOfClusters ( 70.0 ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( 4.0 ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( 0.008 ) ;
        lCascadeResult[lN]->SetCutBachBaryonCosPA       ( TMath::Cos(0.04) ) ; //+variable
        lCascadeResult[lN]->SetCutVarBBCosPA            (TMath::Exp(-2.29048),
                                                         -20.2016,
                                                         TMath::Exp(-2.9581),
                                                         -0.649153,
                                                         0.00526455);
        //Add result to pool
        lN++;
    }
    
    //Will now proceed to sweep individual variables
    
    //________________________________________________________
    // Variable 1: DCA Neg to PV
    Float_t lMaxDCANegToPV = 1.5;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCANegToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCANegToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCANegToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 2: DCA Pos to PV
    Float_t lMaxDCAPosToPV = 1.5;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAPosToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAPosToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCAPosToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 3: DCA V0 Daughters
    Float_t lMaxDCAV0Daughters = 1.40;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAV0DaughtersSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAV0Daughters / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCAV0Daughters ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 4: V0 CosPA
    Float_t lMinV0CosPA = 0.95;
    Float_t lMaxV0CosPA = 1.00;
    Double_t lV0CosPAVals[lNumberOfSteps];
    Double_t lMinV0PA = 0.0;
    Double_t lMaxV0PA = TMath::ACos(lMinV0CosPA);
    Double_t lDeltaV0PA = lMaxV0PA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lV0CosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaV0PA );
    }
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0CosPASweep",icut) );
            //Add result to pool
            lCascadeResult[lN] -> SetCutUseVarV0CosPA( kFALSE );
            lCascadeResult[lN] -> SetCutV0CosPA ( lV0CosPAVals[icut] );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 5: V0 Radius
    Float_t lMinV0Radius = 0.0;
    Float_t lMaxV0Radius = 20.00;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0RadiusSweep",icut) );
            //Add result to pool
            Float_t lThisCut = lMinV0Radius + (lMaxV0Radius-lMinV0Radius)*(((Float_t)icut)+1)/((Float_t)lNumberOfSteps);
            lCascadeResult[lN] -> SetCutV0Radius ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 6:
    Float_t lMaxDCAV0ToPV = 0.5;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAV0ToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAV0ToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCAV0ToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 7: DCA Bach To PV
    Float_t lMaxDCABachToPV = 0.5;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCABachToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCABachToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCABachToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 8: DCA Casc Daughters
    Float_t lMaxDCACascDaughters = 1.40;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCACascDaughtersSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCACascDaughters / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCACascDaughters ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 9: Cascade Radius
    Float_t lMinCascRadius = 0.5;
    Float_t lMaxCascRadius = 7.0;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"CascRadiusSweep",icut) );
            //Add result to pool
            Float_t lThisCut = lMinCascRadius + (lMaxCascRadius-lMinCascRadius)*(((Float_t)icut)+1)/((Float_t)lNumberOfSteps);
            lCascadeResult[lN] -> SetCutCascRadius ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 10: Cascade CosPA
    Float_t lMinCascCosPA = 0.95;
    Float_t lMaxCascCosPA = 1.00;
    Double_t lCascCosPAVals[lNumberOfSteps];
    Double_t lMinCascPA = 0.0;
    Double_t lMaxCascPA = TMath::ACos(lMinCascCosPA);
    Double_t lDeltaCascPA = lMaxCascPA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lCascCosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaCascPA );
    }
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"CascCosPASweep",icut) );
            //Add result to pool
            lCascadeResult[lN] -> SetCutUseVarCascCosPA( kFALSE );
            lCascadeResult[lN] -> SetCutCascCosPA ( lCascCosPAVals[icut] );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 11: Bach-Baryon CosPA
    Float_t lMinBBCosPA = TMath::Cos(0.1);
    Float_t lMaxBBCosPA = 1.000;
    Double_t lBBCosPAVals[lNumberOfSteps];
    Double_t lMinBBPA = 0.0;
    Double_t lMaxBBPA = TMath::ACos(lMinBBCosPA);
    Double_t lDeltaBBPA = lMaxBBPA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lBBCosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaBBPA );
    }
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"BBCosPASweep",icut) );
            //Add result to pool
            lCascadeResult[lN] -> SetCutUseVarBBCosPA( kFALSE );
            lCascadeResult[lN] -> SetCutBachBaryonCosPA ( lBBCosPAVals[icut] );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 12: Cascade Lifetime Sweep
    
    Int_t lLifetimeSteps = 15;
    for(Int_t i = 0 ; i < 4 ; i ++){
        Float_t lMinLifetime = 5.00;
        Float_t lMaxLifetime = 20.00;
        for(Int_t icut = 0; icut<lLifetimeSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"CascLifetimeSweep",icut) );
            Float_t lThisCut = lMinLifetime + (lMaxLifetime-lMinLifetime)*(((Float_t)icut)+1)/((Float_t)lLifetimeSteps);
            //Add result to pool
            lCascadeResult[lN] -> SetCutProperLifetime ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 13: V0 Lifetime Sweep
    Float_t lMinV0Lifetime = 8.00;
    Float_t lMaxV0Lifetime = 40.00;
    Int_t lV0LifetimeSteps = 32;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lV0LifetimeSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"MaxV0LifetimeSweep",icut) );
            Float_t lThisCut = lMinV0Lifetime + (lMaxV0Lifetime-lMinV0Lifetime)*(((Float_t)icut)+1)/((Float_t)lV0LifetimeSteps);
            //Add result to pool
            lCascadeResult[lN] -> SetCutMaxV0Lifetime ( lThisCut );
            lN++;
        }
    }
    
    for (Int_t iconf = 0; iconf<lN; iconf++)
    AddConfiguration(lCascadeResult[iconf]);
    
    cout<<"Added "<<lN<<" Cascade configurations to output."<<endl;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddStandardV0Configuration(Bool_t lUseFull)
//Meant to add some standard V0 analysis Configuration + its corresponding systematics
{
    //======================================================
    // V0 Configurations To use
    //======================================================
    
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimitsV0[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 14, 15, 17, 20};
    Long_t lPtbinnumbV0 = sizeof(lPtbinlimitsV0)/sizeof(Double_t) - 1;
    Double_t lPtbinlimitsXi[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 14, 16, 19, 22, 25};
    Long_t lPtbinnumbXi = sizeof(lPtbinlimitsXi)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimitsV0[] = {0, 1, 5, 10, 20, 30, 40, 50, 60, 70, 80, 85, 90};
    Long_t lCentbinnumbV0 = sizeof(lCentbinlimitsV0)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleNameV0[] =
    {
        "K0Short",
        "Lambda",
        "AntiLambda"
    };
    const Int_t lNPart = 3;
    TString lConfNameV0[] =
    {
        "Loose",
        "Central",
        "Tight"
    };
    const Int_t lNConf = 3;
    TString lCutNameV0[] =
    {
        "DCANegToPV",
        "DCAPosToPV",
        "DCAV0Daughters",
        "V0CosPA",
        "V0Radius",
        "ProperLifetime",
        "TrackLength",
        "LeastNbrCrsOvFind",
        "TPCdEdx",
        "APParameter",
        "V0RadiusMax",
        "LeastNbrCrsRows"
    };
    const Int_t lNCutsForSyst = 10;
    
    // STEP 2: Decide on a set of selections
    
    //1st index: Particle Species
    //2nd index: Loose / Central / Tight
    //3rd index: Number of selection (as ordered above)
    Double_t lcutsV0[lNPart][lNConf][lNCutsForSyst];
    
    //1st index: Particle Species: K0Short, Lambda, AntiLambda
    //2nd index: Loose / Central / Tight: 2%, 5%, 10% signal loss
    Double_t parExp0Const[lNPart][lNConf];
    Double_t parExp0Slope[lNPart][lNConf];
    Double_t parExp1Const[lNPart][lNConf];
    Double_t parExp1Slope[lNPart][lNConf];
    Double_t parConst[lNPart][lNConf];
    
    //=============================================================================================
    // K0SHORT V0 COS PA PARAMETRIZATION
    //---------------------------------------------------------------------------------------------
    //                       LOOSE                         CENTRAL                           TIGHT
    parExp0Const[0][0] =  0.20428;  parExp0Const[0][1] =  0.22692;  parExp0Const[0][2] =  0.28814;
    parExp0Slope[0][0] = -0.73728;  parExp0Slope[0][1] = -1.59317;  parExp0Slope[0][2] = -2.27069;
    parExp1Const[0][0] =  0.09887;  parExp1Const[0][1] =  0.05994;  parExp1Const[0][2] =  0.04320;
    parExp1Slope[0][0] = -0.02822;  parExp1Slope[0][1] = -0.26997;  parExp1Slope[0][2] = -0.29839;
    parConst[0][0] = -0.05302;      parConst[0][1] =  0.00907;      parConst[0][2] =  0.00704;
    //=============================================================================================
    
    //=============================================================================================
    // LAMBDA V0 COS PA PARAMETRIZATION
    //---------------------------------------------------------------------------------------------
    //                       LOOSE                         CENTRAL                           TIGHT
    parExp0Const[1][0] =  0.22775;  parExp0Const[1][1] =  0.36284;  parExp0Const[1][2] =  0.54877;
    parExp0Slope[1][0] = -1.11579;  parExp0Slope[1][1] = -1.87960;  parExp0Slope[1][2] = -2.72912;
    parExp1Const[1][0] =  0.06266;  parExp1Const[1][1] =  0.04543;  parExp1Const[1][2] =  0.03411;
    parExp1Slope[1][0] = -0.17086;  parExp1Slope[1][1] = -0.20447;  parExp1Slope[1][2] = -0.26965;
    parConst[1][0] =  0.01489;      parConst[1][1] =  0.01085;      parConst[1][2] =  0.00889;
    //=============================================================================================
    
    //=============================================================================================
    // ANTILAMBDA V0 COS PA PARAMETRIZATION
    //---------------------------------------------------------------------------------------------
    //                       LOOSE                         CENTRAL                           TIGHT
    parExp0Const[2][0] =  0.22667;  parExp0Const[2][1] =  0.35809;  parExp0Const[2][2] =  0.54114;
    parExp0Slope[2][0] = -0.93618;  parExp0Slope[2][1] = -1.93860;  parExp0Slope[2][2] = -2.71000;
    parExp1Const[2][0] =  0.06857;  parExp1Const[2][1] =  0.05306;  parExp1Const[2][2] =  0.03664;
    parExp1Slope[2][0] = -0.07015;  parExp1Slope[2][1] = -0.24518;  parExp1Slope[2][2] = -0.28124;
    parConst[2][0] = -0.00707;      parConst[2][1] =  0.01213;      parConst[2][2] =  0.00905;
    //=============================================================================================
    
    //================================================================================
    // K0SHORT SELECTIONS
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcutsV0[0][0][ 0] = 0.05;    lcutsV0[0][1][ 0] =  0.10; lcutsV0[0][2][0] = 0.17; //DCANegToPV
    lcutsV0[0][0][ 1] = 0.05;    lcutsV0[0][1][ 1] =  0.10; lcutsV0[0][2][1] = 0.17; //DCAPosToPV
    lcutsV0[0][0][ 2] = 0.95;    lcutsV0[0][1][ 2] =   0.8; lcutsV0[0][2][2] =  0.7; //DCAV0Daughters
    lcutsV0[0][0][ 3] = 0.95;    lcutsV0[0][1][ 3] =  0.95; lcutsV0[0][2][3] = 0.95; //V0CosPA
    lcutsV0[0][0][ 4] = 4.50;    lcutsV0[0][1][ 4] =  5.00; lcutsV0[0][2][4] = 5.50; //V0Radius
    lcutsV0[0][0][ 5] =   25;    lcutsV0[0][1][ 5] =    20; lcutsV0[0][2][5] =   15; //Proper Lifetime (in cm)
    lcutsV0[0][0][ 6] =   80;    lcutsV0[0][1][ 6] =    90; lcutsV0[0][2][6] =  100; //Track Length
    lcutsV0[0][0][ 7] =  0.7;    lcutsV0[0][1][ 7] =   0.8; lcutsV0[0][2][7] = 0.85; //Least Ratio CrdRows/Findable
    lcutsV0[0][0][ 8] =  4.0;    lcutsV0[0][1][ 8] =   3.0; lcutsV0[0][2][8] =  2.5; //TPC dE/dx
    lcutsV0[0][0][ 9] = 0.18;    lcutsV0[0][1][ 9] =  0.20; lcutsV0[0][2][9] = 0.22; //AP Parameter
    //================================================================================
    
    //================================================================================
    // LAMBDA SELECTIONS
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcutsV0[1][0][ 0] = 0.10;    lcutsV0[1][1][ 0] =  0.25; lcutsV0[1][2][0] = 0.40; //DCANegToPV
    lcutsV0[1][0][ 1] = 0.08;    lcutsV0[1][1][ 1] =  0.10; lcutsV0[1][2][1] = 0.13; //DCAPosToPV
    lcutsV0[1][0][ 2] =  1.0;    lcutsV0[1][1][ 2] =   0.8; lcutsV0[1][2][2] = 0.65; //DCAV0Daughters
    lcutsV0[1][0][ 3] = 0.97;    lcutsV0[1][1][ 3] =  0.98; lcutsV0[1][2][3] = 0.99; //V0CosPA
    lcutsV0[1][0][ 4] = 4.00;    lcutsV0[1][1][ 4] =  5.00; lcutsV0[1][2][4] = 6.00; //V0Radius
    lcutsV0[1][0][ 5] =   30;    lcutsV0[1][1][ 5] =    25; lcutsV0[1][2][5] =   20; //Proper Lifetime (in cm)
    lcutsV0[1][0][ 6] =   80;    lcutsV0[1][1][ 6] =    90; lcutsV0[1][2][6] =  100; //Track Length
    lcutsV0[1][0][ 7] =  0.7;    lcutsV0[1][1][ 7] =   0.8; lcutsV0[1][2][7] = 0.85; //Least Ratio CrdRows/Findable
    lcutsV0[1][0][ 8] =  4.0;    lcutsV0[1][1][ 8] =   3.0; lcutsV0[1][2][8] =  2.5; //TPC dE/dx
    lcutsV0[1][0][ 9] = 0.18;    lcutsV0[1][1][ 9] =  0.20; lcutsV0[1][2][9] = 0.22; //AP Parameter
    //================================================================================
    
    //================================================================================
    // ANTILAMBDA SELECTIONS
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcutsV0[2][0][ 0] = 0.08;    lcutsV0[2][1][ 0] =  0.10; lcutsV0[2][2][0] = 0.13; //DCANegToPV
    lcutsV0[2][0][ 1] = 0.10;    lcutsV0[2][1][ 1] =  0.25; lcutsV0[2][2][1] = 0.40; //DCAPosToPV
    lcutsV0[2][0][ 2] =  1.0;    lcutsV0[2][1][ 2] =   0.8; lcutsV0[2][2][2] = 0.65; //DCAV0Daughters
    lcutsV0[2][0][ 3] = 0.97;    lcutsV0[2][1][ 3] =  0.98; lcutsV0[2][2][3] = 0.99; //V0CosPA
    lcutsV0[2][0][ 4] = 4.00;    lcutsV0[2][1][ 4] =  5.00; lcutsV0[2][2][4] = 6.00; //V0Radius
    lcutsV0[2][0][ 5] =   30;    lcutsV0[2][1][ 5] =    25; lcutsV0[2][2][5] =   20; //Proper Lifetime (in cm)
    lcutsV0[2][0][ 6] =   80;    lcutsV0[2][1][ 6] =    90; lcutsV0[2][2][6] =  100; //Track Length
    lcutsV0[2][0][ 7] =  0.7;    lcutsV0[2][1][ 7] =   0.8; lcutsV0[2][2][7] = 0.85; //Least Ratio CrdRows/Findable
    lcutsV0[2][0][ 8] =  4.0;    lcutsV0[2][1][ 8] =   3.0; lcutsV0[2][2][8] =  2.5; //TPC dE/dx
    lcutsV0[2][0][ 9] = 0.18;    lcutsV0[2][1][ 9] =  0.20; lcutsV0[2][2][9] = 0.22; //AP Parameter
    //================================================================================
    
    //STEP 3: Creation of output objects
    
    //Map to mass hypothesis
    AliV0Result::EMassHypo lMassHypoV0[lNPart];
    lMassHypoV0[0] = AliV0Result::kK0Short;
    lMassHypoV0[1] = AliV0Result::kLambda;
    lMassHypoV0[2] = AliV0Result::kAntiLambda;
    
    //Array of results
    AliV0Result *lV0Result[1000];
    Long_t lNV0 = 0;
    
    //Central results: Stored in indices 0, 1, 2 (careful!)
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Central result, customized binning: the one to use, usually
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central",lParticleNameV0[i].Data() ),lMassHypoV0[i],"",lCentbinnumbV0,lCentbinlimitsV0, lPtbinnumbV0,lPtbinlimitsV0);
        lV0Result[lNV0] -> InitializeProtonProfile( lPtbinnumbV0,lPtbinlimitsV0 ); //profile
        
        //feeddown matrix
        if ( i!=0 ) lV0Result[lNV0] -> InitializeFeeddownMatrix( lPtbinnumbV0,lPtbinlimitsV0, lPtbinnumbXi,lPtbinlimitsXi, lCentbinnumbV0,lCentbinlimitsV0);
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][1][ 0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][1][ 1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][1][ 2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][1][ 3] ) ;
        //Set Variable cut
        lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][1], parExp0Slope[i][1], parExp1Const[i][1], parExp1Slope[i][1], parConst[i][1] ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][1][ 4] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][1][ 5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( -1 ) ; //no cut here
        lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][1][ 6] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][1][ 7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][1][ 8] ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][1][ 9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Central full results
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Central Result, Full: No rebinning. Will use a significant amount of memory,
        //not to be replicated several times for systematics!
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central_Full",lParticleNameV0[i].Data() ),lMassHypoV0[i]);
        lV0Result[lNV0] -> InitializeProtonProfile( lPtbinnumbV0,lPtbinlimitsV0 ); //profile
        
        //feeddown matrix
        if ( i!=0 ) lV0Result[lNV0] -> InitializeFeeddownMatrix( lPtbinnumbV0,lPtbinlimitsV0, lPtbinnumbXi,lPtbinlimitsXi, lCentbinnumbV0,lCentbinlimitsV0);
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][1][ 0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][1][ 1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][1][ 2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][1][ 3] ) ;
        //Set Variable cut
        lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][1], parExp0Slope[i][1], parExp1Const[i][1], parExp1Slope[i][1], parConst[i][1] ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][1][ 4] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][1][ 5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( -1 ) ; //no cut here
        lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][1][ 6] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][1][ 7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][1][ 8] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Rapidity sweep
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        for(Int_t ir=0; ir<12; ir++)
        {
            Float_t lLoRap = -0.6 +   (ir)*0.1;
            Float_t lHiRap = -0.6 + (ir+1)*0.1;
            
            //Create a new object from default
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_RapiditySweep_%.1f_%.1f",lParticleNameV0[i].Data(),lLoRap, lHiRap) );
            lV0Result[lNV0] -> SetCutMinRapidity( lLoRap );
            lV0Result[lNV0] -> SetCutMaxRapidity( lHiRap );
            
            //Add result to pool
            lNV0++;
        }
    }
    
    //Number of crossed rows cut
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Create a new object from default
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s",lParticleNameV0[i].Data(),"NCrossedRowsCut") );
        
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( 70 ) ;
        lV0Result[lNV0]->SetCutMinTrackLength ( -1 ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //No Armenteros-Podolanski cut
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Create a new object from default
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s",lParticleNameV0[i].Data(),"NoAP") );
        lV0Result[lNV0]->SetCutArmenterosParameter(0.0);
        
        //Add result to pool
        lNV0++;
    }
    
    //Explore Use MC Properties vs Use Reconstructed Properties
    for(Int_t i = 0 ; i < lNPart ; i ++){
        //Create a new object from default
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_Central_MCUseRecoProp",lParticleNameV0[i].Data() ) );
        
        lV0Result[lNV0]->SetCutMCUseMCProperties(kFALSE);
        
        //Add result to pool
        lNV0++;
    }
    
    //================================================================================
    //Set up cut values, tight and loose versions
    //--------------------------------------------------------------------------------
    const Int_t lNCutsForSweep = 12;
    //1st index: Particle Species
    //2nd index: Number of selection
    Double_t lCutsTight[lNPart][lNCutsForSweep];
    Double_t lCutsLoose[lNPart][lNCutsForSweep];
    
    Double_t lMeanLifetime[] = {2.6844, 7.89, 7.89};
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        lCutsTight[i][ 0] =   0.1; //DCANegToPV
        lCutsTight[i][ 1] =   0.1; //DCAPosToPV
        lCutsTight[i][ 2] =     1; //DCAV0Daughters
        lCutsTight[i][ 3] = 0.998; //V0CosPA
        lCutsTight[i][ 4] =     5; //V0Radius
        lCutsTight[i][ 5] = 3 * lMeanLifetime[i]; //Proper Lifetime (in cm)
        lCutsTight[i][ 6] =    -1; //Track Length
        lCutsTight[i][ 7] = -0.01; //Least Ratio CrdRows/Findable
        lCutsTight[i][ 8] =     8; //TPC dE/dx
        lCutsTight[i][ 9] =   0.2; //AP Parameter
        lCutsTight[i][10] =   100; //V0Radius max
        lCutsTight[i][11] =    70; //Least number of CrdRows
        
        for(Int_t j = 0; j < lNCutsForSyst; j++)
        {
            lCutsLoose[i][j] = lcutsV0[i][1][j];
        }
        lCutsLoose[i][10] =   200; //V0Radius max
        lCutsLoose[i][11] =    -1; //Least number of CrdRows
    }
    //================================================================================
    
    //2.76 TeV analysis cuts
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_276Cuts",lParticleNameV0[i].Data() ) );
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lCutsTight[i][0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutsTight[i][1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutsTight[i][2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lCutsTight[i][3] ) ;
        //Use constant cos(PA) cut
        lV0Result[lNV0]->SetCutUseVarV0CosPA         ( kFALSE ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lCutsTight[i][4] ) ;
        lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutsTight[i][10] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lCutsTight[i][5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutsTight[i][11] ) ;
        lV0Result[lNV0]->SetCutMinTrackLength ( lCutsTight[i][6] ) ; //no cut here
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutsTight[i][7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( 1e6 ) ; //no cut here
        lV0Result[lNV0]->SetCut276TeVLikedEdx        ( kTRUE ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lCutsTight[i][9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    // centrality binning for sweeps
    Double_t lSweepCentBinLimits[] = {0, 90};
    Long_t lSweepCentBinNumb = sizeof(lSweepCentBinLimits)/sizeof(Double_t) - 1;
    
    //Mass binning for sweeps
    Double_t lNMassBins[] = {400, 400, 400};
    Double_t lMass[] = {0.498, 1.116, 1.116};
    Double_t lMassWindow[] = {0.15,0.1,0.1};
    
    //Loose cuts for sweeps
    Int_t lLooseForSweepIndex = lNV0;
    for(Int_t i = 0 ; i < lNPart ; i++)
    {
        //Central result for sweeps
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central_ForSweep",lParticleNameV0[i].Data() ),lMassHypoV0[i],"",lSweepCentBinNumb,lSweepCentBinLimits, lPtbinnumbV0,lPtbinlimitsV0,lNMassBins[i],lMass[i]-lMassWindow[i],lMass[i]+lMassWindow[i]);
        lV0Result[lNV0] -> InitializeProtonProfile( lPtbinnumbV0,lPtbinlimitsV0 ); //profile
        
        //feeddown matrix
        if ( i!=0 ) lV0Result[lNV0] -> InitializeFeeddownMatrix( lPtbinnumbV0,lPtbinlimitsV0, lPtbinnumbXi,lPtbinlimitsXi, lSweepCentBinNumb,lSweepCentBinLimits);
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][1][ 0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][1][ 1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][1][ 2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][1][ 3] ) ;
        //Set Variable cut
        lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][1], parExp0Slope[i][1], parExp1Const[i][1], parExp1Slope[i][1], parConst[i][1] ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][1][ 4] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][1][ 5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( -1 ) ; //no cut here
        lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][1][ 6] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][1][ 7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][1][ 8] ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][1][ 9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Tightening cuts one by one
    for(Int_t i = 0 ; i < lNPart ; i++)
    {
        for(Int_t iCut = 0; iCut < lNCutsForSweep; iCut++)
        {
            //only proceed if cuts are actually different
            if( ( TMath::Abs( lCutsTight[i][iCut]-lCutsLoose[i][iCut] ) / lCutsLoose[i][iCut] < 0.01 ) && ( iCut != 3 ) ) continue;
            
            Int_t lNSweep = 12;
            for(Int_t iSweep = 1; iSweep <= lNSweep; iSweep++)
            {
                Double_t lCutValue = lCutsLoose[i][iCut] + (iSweep/(Double_t)lNSweep)*(lCutsTight[i][iCut]-lCutsLoose[i][iCut]);
                
                lV0Result[lNV0] = new AliV0Result( lV0Result[lLooseForSweepIndex+i], Form( "%s_Central_%s_%d", lParticleNameV0[i].Data(), lCutNameV0[iCut].Data(), iSweep ) );
                
                if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lCutValue ) ;
                if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutValue ) ;
                if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutValue ) ;
                if(iCut ==  3 )
                {
                    lV0Result[lNV0]->SetCutV0CosPA               ( lCutValue ) ;
                    lV0Result[lNV0]->SetCutVarV0CosPA            ( parExp0Const[i][1]*(1-iSweep/(Double_t)lNSweep), parExp0Slope[i][1], parExp1Const[i][1]*(1-iSweep/(Double_t)lNSweep), parExp1Slope[i][1], parConst[i][1] + (iSweep/(Double_t)lNSweep)*(TMath::ACos(lCutsTight[i][iCut])-parConst[i][1]) ) ;
                }
                if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lCutValue ) ;
                
                //Miscellaneous
                if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lCutValue ) ;
                if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lCutValue ) ;
                if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutValue ) ;
                if(iCut ==  8 ) lV0Result[lNV0]->SetCutTPCdEdx               ( lCutValue ) ;
                if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter   ( lCutValue ) ;
                if(iCut == 10 ) lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutValue ) ;
                if(iCut == 11 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutValue ) ;
                
                //Print this variation, add to pool
                lV0Result[lNV0]->Print();
                lNV0++;
            }
        }
    }
    
    //Tight cuts for sweeps
    Int_t lTightForSweepIndex = lNV0;
    for(Int_t i = 0 ; i < lNPart ; i++)
    {
        lV0Result[lNV0] = new AliV0Result( lV0Result[lLooseForSweepIndex+i], Form("%s_276Cuts_ForSweep",lParticleNameV0[i].Data() ) );
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lCutsTight[i][0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutsTight[i][1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutsTight[i][2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lCutsTight[i][3] ) ;
        //Use constant cos(PA) cut
        lV0Result[lNV0]->SetCutUseVarV0CosPA         ( kFALSE ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lCutsTight[i][4] ) ;
        lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutsTight[i][10] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lCutsTight[i][5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutsTight[i][11] ) ;
        lV0Result[lNV0]->SetCutMinTrackLength ( lCutsTight[i][6] ) ; //no cut here
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutsTight[i][7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( 1e6 ) ; //no cut here
        lV0Result[lNV0]->SetCut276TeVLikedEdx        ( kTRUE ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lCutsTight[i][9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Loosening cuts one by one
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        for(Int_t iCut = 0; iCut < lNCutsForSweep; iCut++)
        {
            //only proceed if cuts are actually different
            if( ( TMath::Abs( lCutsTight[i][iCut]-lCutsLoose[i][iCut] ) / lCutsLoose[i][iCut] < 0.01 ) && ( iCut != 3 ) ) continue;
            
            Int_t lNSweep = 12;
            for(Int_t iSweep = 1; iSweep <= lNSweep; iSweep++)
            {
                Double_t lCutValue = lCutsTight[i][iCut] + (iSweep/(Double_t)lNSweep)*(lCutsLoose[i][iCut]-lCutsTight[i][iCut]);
                
                lV0Result[lNV0] = new AliV0Result( lV0Result[lTightForSweepIndex+i], Form( "%s_276Cuts_%s_%d", lParticleNameV0[i].Data(), lCutNameV0[iCut].Data(), iSweep ) );
                
                if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lCutValue ) ;
                if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutValue ) ;
                if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutValue ) ;
                if(iCut ==  3 )
                {
                    lV0Result[lNV0]->SetCutV0CosPA               ( lCutValue ) ;
                    lV0Result[lNV0]->SetCutVarV0CosPA            ( parExp0Const[i][1]*(iSweep/(Double_t)lNSweep), parExp0Slope[i][1], parExp1Const[i][1]*(iSweep/(Double_t)lNSweep), parExp1Slope[i][1], TMath::ACos(lCutsTight[i][iCut]) + (iSweep/(Double_t)lNSweep)*(parConst[i][1]-TMath::ACos(lCutsTight[i][iCut])) ) ;
                }
                if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lCutValue ) ;
                
                //Miscellaneous
                if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lCutValue ) ;
                if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lCutValue ) ;
                if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutValue ) ;
                if(iCut ==  8 )
                {
                    lV0Result[lNV0]->SetCut276TeVLikedEdx        ( kTRUE ) ;
                    lV0Result[lNV0]->SetCutTPCdEdx               ( lCutValue ) ;
                }
                if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter   ( lCutValue ) ;
                if(iCut == 10 ) lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutValue ) ;
                if(iCut == 11 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutValue ) ;
                
                //Print this variation, add to pool
                lV0Result[lNV0]->Print();
                lNV0++;
            }
        }
    }
    
    // STEP 4: Creation of objects to be used in systematics
    // Optimized via use of copy constructors
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        for(Int_t iCut = 0 ; iCut < lNCutsForSyst ; iCut ++)
        {
            //LOOSE VARIATIONS
            //Create a new object from default
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%s",lParticleNameV0[i].Data(),lCutNameV0[iCut].Data(),lConfNameV0[0].Data()) );
            
            if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  3 )
            {
                lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][0][iCut] ) ;
                lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][0], parExp0Slope[i][0], parExp1Const[i][0], parExp1Slope[i][0], parConst[i][0] ) ;
            }
            if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][0][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][0][ iCut] ) ;
            if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  8 ) lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][0][iCut] ) ;
            
            //Print this variation, add to pool
            lV0Result[lNV0]->Print();
            lNV0++;
            
            //TIGHT VARIATIONS
            //Create a new object from default
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%s",lParticleNameV0[i].Data(),lCutNameV0[iCut].Data(),lConfNameV0[2].Data()) );
            
            if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  3 )
            {
                lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][2][iCut] ) ;
                lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][2], parExp0Slope[i][2], parExp1Const[i][2], parExp1Slope[i][2], parConst[i][2] ) ;
            }
            if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][2][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][2][ iCut] ) ;
            if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  8 ) lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][2][iCut] ) ;
            
            //Print this variation, add to pool
            lV0Result[lNV0]->Print();
            lNV0++;
        }
    }
    
    for (Int_t iconf = 0; iconf<lNV0; iconf++){
        cout<<"Adding config named"<<lV0Result[iconf]->GetName()<<endl;
        AddConfiguration(lV0Result[iconf]);
    }
    
    cout<<"Added "<<lNV0<<" V0 configurations to output."<<endl;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddStandardCascadeConfiguration(Bool_t lUseFull)
//Meant to add some standard cascade analysis Configuration + its corresponding systematics
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] = {0.4, 0.5, 0.6,
        0.7,0.8,.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,
        2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,3.0,3.2,3.4,3.6,3.8,4.0,4.2,
        4.4,4.5,4.6,4.8,5.0,5.5,6.0,6.5,7.0,8.0,9.0,10.,11.,12.};
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleName[] = {"XiMinus", "XiPlus",  "OmegaMinus", "OmegaPlus"};
    TString lConfName[]     = {"Loose",   "Central", "Tight"     };
    TString lCutName[]      = {
        "DCANegToPV", //1
        "DCAPosToPV", //2
        "DCAV0Daughters", //3
        "V0Radius", //4
        "DCAV0ToPV", //5
        "V0Mass", //6
        "DCABachToPV", //7
        "DCACascDaughters", //8
        "CascRadius", //9
        "ProperLifetime", //10
        "ProperLifetimeV0", //11
        "MinLength", //12
        "TPCdEdx", //13
        "Competing", //14
        "DCA3DCascToPV" //15
    };
    
    // STEP 2: Decide on a set of selections
    
    //1st index: Particle Species
    //2nd index: Loose / Central / Tight
    //3rd index: Number of selection (as ordered above)
    Double_t lcuts[4][3][15];
    
    //N.B.: These are mostly symmetric, except for the proper lifetime, which is different
    //      for the two particle species. Another potential improvement could be asymmetric
    //      DCA selections for the Neg / Pos tracks for the (anti)Lambda decay, as decay
    //      kinematics would prefer having the pion daughter with a larger DCA.
    
    Int_t lIdx = 0;
    
    //================================================================================
    // XIMINUS SELECTIONS
    lIdx = 0; //Master XiMinus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.20;    lcuts[lIdx][1][ 7] =   1.0; lcuts[lIdx][2][ 7] =   0.8; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.8;    lcuts[lIdx][1][ 8] =   1.2; lcuts[lIdx][2][ 8] =  3.00; //CascRadius 9
    lcuts[lIdx][0][ 9] = 17.5;    lcuts[lIdx][1][ 9] =  15.0; lcuts[lIdx][2][ 9] =  12.5; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  1.2;    lcuts[lIdx][1][14] =   0.8; lcuts[lIdx][2][14] =   0.6; //3D DCA Cascade To PV
    //================================================================================
    
    //================================================================================
    // XIPLUS SELECTIONS
    lIdx = 1; //Master XiPlus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.20;    lcuts[lIdx][1][ 7] =   1.0; lcuts[lIdx][2][ 7] =   0.8; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.8;    lcuts[lIdx][1][ 8] =   1.2; lcuts[lIdx][2][ 8] =  3.00; //CascRadius 9
    lcuts[lIdx][0][ 9] = 17.5;    lcuts[lIdx][1][ 9] =  15.0; lcuts[lIdx][2][ 9] =  12.5; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  1.2;    lcuts[lIdx][1][14] =   0.8; lcuts[lIdx][2][14] =   0.6; //3D DCA Cascade To PV
    //================================================================================
    
    //================================================================================
    // OMEGAMINUS SELECTIONS
    lIdx = 2; //Master OmegaMinus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.00;    lcuts[lIdx][1][ 7] =   0.6; lcuts[lIdx][2][ 7] =   0.5; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.6;    lcuts[lIdx][1][ 8] =   1.0; lcuts[lIdx][2][ 8] =  2.50; //CascRadius 9
    lcuts[lIdx][0][ 9] = 14.0;    lcuts[lIdx][1][ 9] =  12.0; lcuts[lIdx][2][ 9] =  10.0; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  0.8;    lcuts[lIdx][1][14] =   0.6; lcuts[lIdx][2][14] =   0.5; //3D DCA Cascade To PV
    //================================================================================
    
    //================================================================================
    // OMEGAPLUS SELECTIONS
    lIdx = 3; //Master OmegaPlus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.00;    lcuts[lIdx][1][ 7] =   0.6; lcuts[lIdx][2][ 7] =   0.5; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.6;    lcuts[lIdx][1][ 8] =   1.0; lcuts[lIdx][2][ 8] =  2.50; //CascRadius 9
    lcuts[lIdx][0][ 9] = 14.0;    lcuts[lIdx][1][ 9] =  12.0; lcuts[lIdx][2][ 9] =  10.0; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  0.8;    lcuts[lIdx][1][14] =   0.6; lcuts[lIdx][2][14] =   0.5; //3D DCA Cascade To PV
    //================================================================================
    
    //STEP 3: Creation of output objects
    
    //Just a counter and one array, please. Nothing else needed
    AliCascadeResult *lCascadeResult[600];
    Long_t lN = 0;
    
    //Map to mass hypothesis
    AliCascadeResult::EMassHypo lMassHypo[4];
    lMassHypo[0] = AliCascadeResult::kXiMinus;
    lMassHypo[1] = AliCascadeResult::kXiPlus;
    lMassHypo[2] = AliCascadeResult::kOmegaMinus;
    lMassHypo[3] = AliCascadeResult::kOmegaPlus;
    
    
    //Central results: Stored in indices 0, 1, 2, 3 (careful!)
    for(Int_t i = 0 ; i < 4 ; i ++){
        //Central result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_Central",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits);
        
        //This is MC: generate profile for G3/F (if ever needed)
        lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
        
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][1][ 0] ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][1][ 1] ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][1][ 2] ) ;
        lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][1][ 3] ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][1][ 4] ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][1][ 5] ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][1][ 6] ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][1][ 7] ) ;
        lCascadeResult[lN]->SetCutVarDCACascDau ( TMath::Exp(0.0470076), -0.917006, 0, 1, 0.5 );
        lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][1][ 8] ) ;
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][1][ 9] ) ;
        lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][1][10] ) ;
        lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][1][11] ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][1][12] ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][1][13] ) ;
        lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][1][14] ) ;
        
        //Parametric angle cut initializations
        //V0 cosine of pointing angle
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.95 ) ; //+variable
        lCascadeResult[lN]->SetCutVarV0CosPA            (TMath::Exp(10.853),
                                                         -25.0322,
                                                         TMath::Exp(-0.843948),
                                                         -0.890794,
                                                         0.057553);
        
        //Cascade cosine of pointing angle
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.95 ) ; //+variable
        if(i < 2){
            lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(4.86664),
                                                             -10.786,
                                                             TMath::Exp(-1.33411),
                                                             -0.729825,
                                                             0.0695724);
        }
        if(i >= 2){
            lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(   12.8752),
                                                             -21.522,
                                                             TMath::Exp( -1.49906),
                                                             -0.813472,
                                                             0.0480962);
        }
        
        //BB cosine of pointing angle
        lCascadeResult[lN]->SetCutBachBaryonCosPA       ( TMath::Cos(0.04) ) ; //+variable
        lCascadeResult[lN]->SetCutVarBBCosPA            (TMath::Exp(-2.29048),
                                                         -20.2016,
                                                         TMath::Exp(-2.9581),
                                                         -0.649153,
                                                         0.00526455);
        
        //Add result to pool
        lN++;
    }
    if ( lUseFull ) {
        //Central Full results: Stored in indices 4, 5, 6, 7 (careful!)
        for(Int_t i = 0 ; i < 4 ; i ++){
            lCascadeResult[lN] = new AliCascadeResult( Form("%s_Central_Full",lParticleName[i].Data() ),lMassHypo[i]);
            
            //This is MC: generate profile for G3/F (if ever needed)
            lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
            
            //Setters for V0 Cuts
            lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][1][ 0] ) ;
            lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][1][ 1] ) ;
            lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][1][ 2] ) ;
            lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][1][ 3] ) ;
            //Setters for Cascade Cuts
            lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][1][ 4] ) ;
            lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][1][ 5] ) ;
            lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][1][ 6] ) ;
            lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][1][ 7] ) ;
            lCascadeResult[lN]->SetCutVarDCACascDau ( TMath::Exp(0.0470076), -0.917006, 0, 1, 0.5 );
            lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][1][ 8] ) ;
            //Miscellaneous
            lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][1][ 9] ) ;
            lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][1][10] ) ;
            lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][1][11] ) ;
            lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][1][12] ) ;
            lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][1][13] ) ;
            lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][1][14] ) ;
            
            //Parametric angle cut initializations
            //V0 cosine of pointing angle
            lCascadeResult[lN]->SetCutV0CosPA               ( 0.95 ) ; //+variable
            lCascadeResult[lN]->SetCutVarV0CosPA            (TMath::Exp(10.853),
                                                             -25.0322,
                                                             TMath::Exp(-0.843948),
                                                             -0.890794,
                                                             0.057553);
            
            //Cascade cosine of pointing angle
            lCascadeResult[lN]->SetCutCascCosPA             ( 0.95 ) ; //+variable
            if(i < 2){
                lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(4.86664),
                                                                 -10.786,
                                                                 TMath::Exp(-1.33411),
                                                                 -0.729825,
                                                                 0.0695724);
            }
            if(i >= 2){
                lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(   12.8752),
                                                                 -21.522,
                                                                 TMath::Exp( -1.49906),
                                                                 -0.813472,
                                                                 0.0480962);
            }
            
            //BB cosine of pointing angle
            lCascadeResult[lN]->SetCutBachBaryonCosPA       ( TMath::Cos(0.04) ) ; //+variable
            lCascadeResult[lN]->SetCutVarBBCosPA            (TMath::Exp(-2.29048),
                                                             -20.2016,
                                                             TMath::Exp(-2.9581),
                                                             -0.649153,
                                                             0.00526455);
            
            //Add result to pool
            lN++;
        }
    }
    
    //Explore restricted rapidity range check
    for(Int_t i = 0 ; i < 4 ; i ++){
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_Central_y03",lParticleName[i].Data() ) );
        
        lCascadeResult[lN] -> SetCutMinRapidity(-0.3);
        lCascadeResult[lN] -> SetCutMaxRapidity(+0.3);
        
        //Add result to pool
        lN++;
    }
    
    Float_t lLowRap = -0.6;
    Float_t lHighRap = -0.5;
    for(Int_t i=0;i<4;i++){
        lLowRap = -0.6;
        lHighRap = -0.5;
        for(Int_t irapbin=0;irapbin<12;irapbin++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%f_%f",lParticleName[i].Data(),"DefaultRapiditySweep",lLowRap,lHighRap ) );
            lCascadeResult[lN]->SetCutMinRapidity(lLowRap);
            lCascadeResult[lN]->SetCutMaxRapidity(lHighRap);
            lN++;
            lLowRap+=0.1;
            lHighRap+=0.1;
        }
    }
    
    // STEP 4: Creation of objects to be used in systematics
    // Optimized via use of copy constructors
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t iCut = 0 ; iCut < 15 ; iCut ++){
            
            //LOOSE VARIATIONS
            //Create a new object from default
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),lCutName[iCut].Data(),lConfName[0].Data()) );
            
            if(iCut ==  0 ) lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][0][iCut] ) ;
            if(iCut ==  1 ) lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][0][iCut] ) ;
            if(iCut ==  2 ) lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][0][iCut] ) ;
            if(iCut ==  3 ) lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][0][iCut] ) ;
            
            //Setters for Cascade Cuts
            if(iCut ==  4 ) lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][0][iCut] ) ;
            if(iCut ==  5 ) lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][0][iCut] ) ;
            if(iCut ==  6 ) lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][0][iCut] ) ;
            if(iCut ==  7 ){
                lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][0][iCut] ) ;
                lCascadeResult[lN]->SetCutVarDCACascDau ( 1.2 * TMath::Exp(0.0470076), -0.917006, 0, 1, 1.2 * 0.5 );
            }
            if(iCut ==  8 ) lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][0][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  9 ) lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][0][iCut] ) ;
            if(iCut == 10 ) lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][0][iCut] ) ;
            if(iCut == 11 ) lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][0][iCut] ) ;
            if(iCut == 12 ) lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][0][iCut] ) ;
            if(iCut == 13 ) lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][0][iCut] ) ;
            if(iCut == 14 ) lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][0][iCut] ) ;
            
            //Print this variation, add to pool
            //lCascadeResult[lN]->Print();
            lN++;
            
            //TIGHT VARIATIONS
            //Create a new object from default
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),lCutName[iCut].Data(),lConfName[2].Data()) );
            
            if(iCut ==  0 ) lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][2][iCut] ) ;
            if(iCut ==  1 ) lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][2][iCut] ) ;
            if(iCut ==  2 ) lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][2][iCut] ) ;
            if(iCut ==  3 ) lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][2][iCut] ) ;
            
            //Setters for Cascade Cuts
            if(iCut ==  4 ) lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][2][iCut] ) ;
            if(iCut ==  5 ) lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][2][iCut] ) ;
            if(iCut ==  6 ) lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][2][iCut] ) ;
            if(iCut ==  7 ){
                lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][2][iCut] ) ;
                lCascadeResult[lN]->SetCutVarDCACascDau ( 0.8 * TMath::Exp(0.0470076), -0.917006, 0, 1, 0.8 * 0.5 );
            }
            if(iCut ==  8 ) lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][2][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  9 ) lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][2][iCut] ) ;
            if(iCut == 10 ) lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][2][iCut] ) ;
            if(iCut == 11 ) lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][2][iCut] ) ;
            if(iCut == 12 ) lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][2][iCut] ) ;
            if(iCut == 13 ) lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][2][iCut] ) ;
            if(iCut == 14 ) lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][2][iCut] ) ;
            
            //Print this variation, add to pool
            //lCascadeResult[lN]->Print();
            lN++;
        }
    }
    
    //STEP 5: re-parametrization of cosines for tight and loose variations (done manually)
    for(Int_t i = 0 ; i < 4 ; i ++){
        //======================================================
        //V0CosPA Variations
        //======================================================
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"V0CosPA","Loose") );
        lCascadeResult[lN]->SetCutVarV0CosPA(TMath::Exp(  -1.77429),
                                             -0.692453,
                                             TMath::Exp( -2.01938),
                                             -0.201574,
                                             0.0776465);
        lN++;
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"V0CosPA","Tight") );
        lCascadeResult[lN]->SetCutVarV0CosPA(TMath::Exp(  -1.21892),
                                             -41.8521,
                                             TMath::Exp(   -1.278),
                                             -0.894064,
                                             0.0303932);
        lN++;
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"V0CosPA","VeryTight") );
        lCascadeResult[lN]->SetCutVarV0CosPA(TMath::Exp(   12.8077),
                                             -21.2944,
                                             TMath::Exp( -1.53357),
                                             -0.920017,
                                             0.0262315);
        
        lN++;
        //======================================================
        //CascCosPA Variations
        //======================================================
        if( i < 2 ){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Loose") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(  -1.77429),
                                                   -0.692453,
                                                   TMath::Exp( -2.01938),
                                                   -0.201574,
                                                   0.0776465);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Tight") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(   12.8752),
                                                   -21.522,
                                                   TMath::Exp( -1.49906),
                                                   -0.813472,
                                                   0.0480962);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","VeryTight") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(    12.801),
                                                   -21.6157,
                                                   TMath::Exp( -1.66297),
                                                   -0.889246,
                                                   0.0346838);
            lN++;
        }
        if( i >= 2 ){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Loose") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(4.86664),
                                                   -10.786,
                                                   TMath::Exp(-1.33411),
                                                   -0.729825,
                                                   0.0695724);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Tight") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(    12.801),
                                                   -21.6157,
                                                   TMath::Exp( -1.66297),
                                                   -0.889246,
                                                   0.0346838);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","VeryTight") );
            lCascadeResult[lN]->SetCutCascCosPA             ( 0.9992 );
            lN++;
        }
        //======================================================
        //BBCosPA Variations
        //======================================================
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"BBCosPA","Loose") );
        lCascadeResult[lN]->SetCutBachBaryonCosPA        ( TMath::Cos(0.03) ) ;
        lCascadeResult[lN]->SetCutVarBBCosPA(TMath::Exp(    -2.8798),
                                             -20.9876,
                                             TMath::Exp(  -3.10847),
                                             -0.73045,
                                             0.00235147);
        lN++;
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"BBCosPA","Tight") );
        lCascadeResult[lN]->SetCutBachBaryonCosPA        ( TMath::Cos(0.05) ) ;
        lCascadeResult[lN]->SetCutVarBBCosPA(TMath::Exp(   12.4606),
                                             -20.578,
                                             TMath::Exp( -2.41442),
                                             -0.709588,
                                             0.01079);
        lN++;
    }
    
    //STEP 6: V0 Mass sweep
    //for(Int_t i = 0 ; i < 4 ; i ++){
    //    for(Int_t isweep=0; isweep<20;isweep++){
    //        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_V0MassSweep_%i",lParticleName[i].Data(),isweep) );
    //        lCascadeResult[lN]->SetCutV0MassSigma( ((Double_t)(isweep)/4000.0)); //in GeV/c^2
    //        lN++;
    //    }
    //}
    
    Float_t lLifetimeCut[4];
    lLifetimeCut[0] = 15.0;
    lLifetimeCut[1] = 15.0;
    lLifetimeCut[2] = 12.0;
    lLifetimeCut[3] = 12.0;
    
    Float_t lMass[4];
    lMass[0] = 1.322;
    lMass[1] = 1.322;
    lMass[2] = 1.672;
    lMass[3] = 1.672;
    
    //Old vertexer-level configuration for cross-checks
    for(Int_t i = 0 ; i < 4 ; i ++){
        //Central result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_VertexerLevel",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits,100,lMass[i]-0.050,lMass[i]+0.050);
        
        
        //This is MC: generate profile for G3/F (if ever needed)
        lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
        
        //Default cuts: use vertexer level ones
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        (  1. ) ;
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.98 ) ;
        lCascadeResult[lN]->SetCutV0Radius              (  3 ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( 0.1 ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( 0.006 ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( 0.03 ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( 1. ) ;
        lCascadeResult[lN]->SetCutCascRadius            ( 1.2 ) ;
        if(i==2||i==3)
        lCascadeResult[lN]->SetCutCascRadius            ( 1.0 ) ; //omega case
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.98 ) ;
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( lLifetimeCut[i] ) ;
        lCascadeResult[lN]->SetCutMinTrackLength           ( 90.0 ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( 4.0 ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( 0.008 ) ;
        lCascadeResult[lN]->SetCutBachBaryonCosPA        ( TMath::Cos(0.006) ) ;
        //Add result to pool
        lN++;
    }
    
    for (Int_t iconf = 0; iconf<lN; iconf++){
        cout<<"["<<iconf<<"/"<<lN<<"] Adding config named "<<lCascadeResult[iconf]->GetName()<<endl;
        AddConfiguration(lCascadeResult[iconf]);
    }
    cout<<"Added "<<lN<<" Cascade configurations to output."<<endl;
}


//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::AddCascadeConfiguration276TeV()
//Adds 2.76 TeV cascade analysis configuration
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] = {0.4, 0.5, 0.6,
        0.7,0.8,.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,
        2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,3.0,3.2,3.4,3.6,3.8,4.0,4.2,
        4.4,4.6,4.8,5.0,5.5,6.0,6.5,7.0,8.0,9.0,10.,11.,12.};
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleName[] = {"XiMinus", "XiPlus",  "OmegaMinus", "OmegaPlus"};
    
    //Just a counter and one array, please. Nothing else needed
    AliCascadeResult *lCascadeResult[100];
    Long_t lN = 0;
    
    //Map to mass hypothesis
    AliCascadeResult::EMassHypo lMassHypo[4];
    lMassHypo[0] = AliCascadeResult::kXiMinus;
    lMassHypo[1] = AliCascadeResult::kXiPlus;
    lMassHypo[2] = AliCascadeResult::kOmegaMinus;
    lMassHypo[3] = AliCascadeResult::kOmegaPlus;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        //2.76 Config result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_276TeV",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits);
        lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
        
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( 0.1    ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( 0.1    ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        ( 0.8    ) ;
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.95   ) ; // + variable
        lCascadeResult[lN]->SetCutUse276TeVV0CosPA      ( kTRUE  ) ;
        lCascadeResult[lN]->SetCutV0Radius              ( 3.0    ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( 0.1    ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( 0.005  ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( 0.03   ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( 0.3    ) ;
        lCascadeResult[lN]->SetCutCascRadius            ( 1.5    ) ;
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.9992 ) ;
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( 15.0   ) ;
        lCascadeResult[lN]->SetCutLeastNumberOfClusters ( 70     ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( 4      ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( 0.008  ) ;
        lCascadeResult[lN]->SetCutDCABachToBaryon       ( 0      ) ;
        
        if(i > 1){
            lCascadeResult[lN]->SetCutCascRadius            ( 1.0 ) ;
            lCascadeResult[lN]->SetCutProperLifetime        ( 8.0 ) ;
        }
        
        //Add result to pool
        lN++;
    }
    
    //Explore restricted rapidity range check
    for(Int_t i = 0 ; i < 4 ; i ++){
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_276TeV_y03",lParticleName[i].Data() ) );
        
        lCascadeResult[lN] -> SetCutMinRapidity(-0.3);
        lCascadeResult[lN] -> SetCutMaxRapidity(+0.3);
        
        //Add result to pool
        lN++;
    }
    
    Float_t lLowRap = -0.6;
    Float_t lHighRap = -0.5;
    for(Int_t i=0;i<4;i++){
        lLowRap = -0.6;
        lHighRap = -0.5;
        for(Int_t irapbin=0;irapbin<12;irapbin++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%f_%f",lParticleName[i].Data(),"276TeVRapiditySweep",lLowRap,lHighRap ) );
            lCascadeResult[lN]->SetCutMinRapidity(lLowRap);
            lCascadeResult[lN]->SetCutMaxRapidity(lHighRap);
            lN++;
            lLowRap+=0.1;
            lHighRap+=0.1;
        }
    }
    
    for (Int_t iconf = 0; iconf<lN; iconf++){
        cout<<"["<<iconf<<"/"<<lN<<"] Adding config named "<<lCascadeResult[iconf]->GetName()<<endl;
        AddConfiguration(lCascadeResult[iconf]);
    }
    
    cout<<"Added "<<lN<<" cascade configurations to output (corresponding to 2.76 TeV analysis cuts)"<<endl;
}


//________________________________________________________________________
Float_t AliAnalysisTaskStrangenessVsMultiplicityRun2::GetDCAz(AliESDtrack *lTrack)
//Encapsulation of DCAz calculation
{
    Float_t b[2];
    Float_t bCov[3];
    lTrack->GetImpactParameters(b,bCov);
    if (bCov[0]<=0 || bCov[2]<=0) {
        AliDebug(1, "Estimated b resolution lower or equal to zero!");
        bCov[0]=0; bCov[2]=0;
    }
    //Float_t dcaToVertexXY = b[0];
    Float_t dcaToVertexZ = b[1];
    
    return dcaToVertexZ;
}


//________________________________________________________________________
Float_t AliAnalysisTaskStrangenessVsMultiplicityRun2::GetCosPA(AliESDtrack *lPosTrack, AliESDtrack *lNegTrack, AliESDEvent *lEvent)
//Encapsulation of CosPA calculation (warning: considers AliESDtrack clones)
{
    Float_t lCosPA = -1;
    
    //Get Magnetic field and primary vertex
    Double_t b=lEvent->GetMagneticField();
    const AliESDVertex *vtxT3D=lEvent->GetPrimaryVertex();
    Double_t xPrimaryVertex=vtxT3D->GetX();
    Double_t yPrimaryVertex=vtxT3D->GetY();
    Double_t zPrimaryVertex=vtxT3D->GetZ();
    
    //Copy AliExternalParam for handling
    AliExternalTrackParam nt(*lNegTrack), pt(*lPosTrack), *lNegClone=&nt, *lPosClone=&pt;
    
    //Find DCA
    Double_t xn, xp, dca=lNegClone->GetDCA(lPosClone,b,xn,xp);
    
    //Propagate to it
    nt.PropagateTo(xn,b); pt.PropagateTo(xp,b);
    
    //Create V0 object to do propagation
    AliESDv0 vertex(nt,1,pt,2); //Never mind indices, won't use
    
    //Get CosPA
    lCosPA=vertex.GetV0CosineOfPointingAngle(xPrimaryVertex,yPrimaryVertex,zPrimaryVertex);
    
    //Return value
    return lCosPA;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityRun2::CheckChargeV0(AliESDv0 *v0)
{
    // This function checks charge of negative and positive daughter tracks.
    // If incorrectly defined (onfly vertexer), swaps out.
    if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() < 0 ) {
        //V0 daughter track swapping is required! Note: everything is swapped here... P->N, N->P
        Long_t lCorrectNidx = v0->GetPindex();
        Long_t lCorrectPidx = v0->GetNindex();
        Double32_t	lCorrectNmom[3];
        Double32_t	lCorrectPmom[3];
        v0->GetPPxPyPz( lCorrectNmom[0], lCorrectNmom[1], lCorrectNmom[2] );
        v0->GetNPxPyPz( lCorrectPmom[0], lCorrectPmom[1], lCorrectPmom[2] );
        
        AliExternalTrackParam	lCorrectParamN(
                                               v0->GetParamP()->GetX() ,
                                               v0->GetParamP()->GetAlpha() ,
                                               v0->GetParamP()->GetParameter() ,
                                               v0->GetParamP()->GetCovariance()
                                               );
        AliExternalTrackParam	lCorrectParamP(
                                               v0->GetParamN()->GetX() ,
                                               v0->GetParamN()->GetAlpha() ,
                                               v0->GetParamN()->GetParameter() ,
                                               v0->GetParamN()->GetCovariance()
                                               );
        lCorrectParamN.SetMostProbablePt( v0->GetParamP()->GetMostProbablePt() );
        lCorrectParamP.SetMostProbablePt( v0->GetParamN()->GetMostProbablePt() );
        
        //Get Variables___________________________________________________
        Double_t lDcaV0Daughters = v0 -> GetDcaV0Daughters();
        Double_t lCosPALocal     = v0 -> GetV0CosineOfPointingAngle();
        Bool_t lOnFlyStatusLocal = v0 -> GetOnFlyStatus();
        
        //Create Replacement Object_______________________________________
        AliESDv0 *v0correct = new AliESDv0(lCorrectParamN,lCorrectNidx,lCorrectParamP,lCorrectPidx);
        v0correct->SetDcaV0Daughters          ( lDcaV0Daughters   );
        v0correct->SetV0CosineOfPointingAngle ( lCosPALocal       );
        v0correct->ChangeMassHypothesis       ( kK0Short          );
        v0correct->SetOnFlyStatus             ( lOnFlyStatusLocal );
        
        //Reverse Cluster info..._________________________________________
        v0correct->SetClusters( v0->GetClusters( 1 ), v0->GetClusters ( 0 ) );
        
        *v0 = *v0correct;
        //Proper cleanup..._______________________________________________
        v0correct->Delete();
        v0correct = 0x0;
        
        //Just another cross-check and output_____________________________
        if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() < 0 ) {
            AliWarning("Found Swapped Charges, tried to correct but something FAILED!");
        } else {
            //AliWarning("Found Swapped Charges and fixed.");
        }
        //________________________________________________________________
    } else {
        //Don't touch it! ---
        //Printf("Ah, nice. Charges are already ordered...");
    }
    return;
}

//______________________________________________________________________
AliAnalysisTaskStrangenessVsMultiplicityRun2::FMDhits AliAnalysisTaskStrangenessVsMultiplicityRun2::GetFMDhits(AliAODEvent* aodEvent) const
// Relies on the event being vaild (no extra checks if object exists done here)
{
    AliAODForwardMult* aodForward = static_cast<AliAODForwardMult*>(aodEvent->FindListObject("Forward"));
    // Shape of d2Ndetadphi: 200, -4, 6, 20, 0, 2pi
    const TH2D& d2Ndetadphi = aodForward->GetHistogram();
    Int_t nEta = d2Ndetadphi.GetXaxis()->GetNbins();
    Int_t nPhi = d2Ndetadphi.GetYaxis()->GetNbins();
    FMDhits ret_vector;
    for (Int_t iEta = 1; iEta <= nEta; iEta++) {
        Int_t valid = Int_t(d2Ndetadphi.GetBinContent(iEta, 0));
        if (!valid) {
            // No data expected for this eta
            continue;
        }
        Float_t eta = d2Ndetadphi.GetXaxis()->GetBinCenter(iEta);
        for (Int_t iPhi = 1; iPhi <= nPhi; iPhi++) {
            // Bin content is most likely number of particles!
            Float_t mostProbableN = d2Ndetadphi.GetBinContent(iEta, iPhi);
            if (mostProbableN > 0) {
                Float_t phi = d2Ndetadphi.GetYaxis()->GetBinCenter(iPhi);
                ret_vector.push_back(AliAnalysisTaskStrangenessVsMultiplicityRun2::FMDhit(eta, phi, mostProbableN));
            }
        }
    }
    return ret_vector;
}
