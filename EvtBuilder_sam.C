#include <stdio.h>
#include <stdlib.h>

#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TMath.h"
#include <TRandom1.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include "TRandom3.h"
#include "TTree.h"
#include "TString.h"
#include "TSystem.h"
#include "TGraph.h"
#include "TChain.h"

//Local Headers
#include "SL_Event.h"
#include "Filter.hh"
#include "FileManager.h"
#include "InputManager.hh"
#include "CorrectionManager.hh"



#define BAD_NUM -10008



struct Sl_Event {
  Int_t channel;
  Double_t time;
  Double_t integral;
  Double_t softwareCFD;
};

Bool_t checkChannels(Int_t a,Int_t b){
  if (TMath::Abs(a-b) == 1)
    return true;
  else
    return false;
}

Bool_t checkChannels(vector <Sl_Event> &in){

  vector <Bool_t> ch(20,false);  //to make this work with different cable arrangements

  for (int i=0;i<in.size();i++){
    if (in[i].channel<14)
      ch[in[i].channel]=true;
  }
  // if it was a good event there should be 4 trues
  int count=0;
  for (int i=0;i <ch.size();i++){
    if (ch[i]==true){
      count++;
    }
  }
  
  if (count == 4)
    return true;
  else 
    return false;

}





using namespace std;


int main(int argc, char **argv){

  vector <string> inputs;
  for (int i=1;i<argc;++i){
    inputs.push_back(string(argv[i]));
  }
  if (inputs.size() == 0 ){ // no argumnets display helpful message
    cout<<"Usage: ./EvtBuilder runNumber [options:value]"<<endl;
    return 0;
  }  
  
  InputManager theInputManager;
  if ( !  theInputManager.loadInputs(inputs) ){
    return 0;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////


  //load correcctions and settings
  
  Double_t sigma=theInputManager.sigma; // the sigma used in the fitting option

  Int_t runNum=theInputManager.runNum;
  Int_t numFiles=theInputManager.numFiles;

  Long64_t maxentry=-1;

  Bool_t makeTraces=theInputManager.makeTraces;

  Bool_t extFlag=theInputManager.ext_flag;
  Bool_t ext_sigma_flag=theInputManager.ext_sigma_flag;

  //defualt Filter settings see pixie manual
  Double_t FL=theInputManager.FL;
  Double_t FG=theInputManager.FG;
  int CFD_delay=theInputManager.d; //in clock ticks
  Double_t CFD_scale_factor =theInputManager.w;
  Bool_t correctionRun =theInputManager.correction;

  CorrectionManager corMan;
  corMan.loadFile(runNum);
  Double_t SDelta_T1_Cor=corMan.get("sdt1");
  Double_t SDelta_T2_Cor=corMan.get("sdt2");
  Double_t int_corrections[4];  
  int_corrections[0]=corMan.get("int0");
  int_corrections[1]=corMan.get("int1");
  int_corrections[2]=corMan.get("int2");
  int_corrections[3]=corMan.get("int3");
  
  int degree=3;
  Double_t GOE_cor1[degree];
  Double_t GOE_cor2[degree];
  Double_t DeltaT_cor1[degree];
  Double_t DeltaT_cor2[degree];
  std::stringstream temp;
  for (int i=1;i<=degree;i++){
    temp.str("");
    temp<<"goe1_"<<i;
    GOE_cor1[i-1]=corMan.get(temp.str().c_str());
    temp.str("");
    temp<<"goe2_"<<i;
    GOE_cor2[i-1]=corMan.get(temp.str().c_str());

    temp.str("");
    temp<<"dt1_"<<i;
    DeltaT_cor1[i-1]=corMan.get(temp.str().c_str());

    temp.str("");
    temp<<"dt2_"<<i;
    DeltaT_cor1[i-1]=corMan.get(temp.str().c_str());
  }



  //prepare files and output tree
  ////////////////////////////////////////////////////////////////////////////////////
  TFile *outFile=0;
  TTree  *outT;
  FileManager * fileMan = new FileManager();
  fileMan->timingMode = theInputManager.timingMode;
  TChain * inT;

  if (!correctionRun){
    inT= new TChain("dchan");
    if (numFiles == -1 ){
      TString s = fileMan->loadFile(runNum,0);
      inT->Add(s);
    } else {
      for (Int_t i=0;i<numFiles;i++) {
	TString s = fileMan->loadFile(runNum,i);
	inT->Add(s);
      }
    }
  } else {
    inT= new TChain("flt");
    inT->Add((TString)theInputManager.specificFileName);
  }
    inT->SetMakeClass(1);
    Long64_t nentry=(Long64_t) (inT->GetEntries());

  cout <<"The number of entires is : "<< nentry << endl ;


  // Openning output Tree and output file
  if (correctionRun)
    outFile=fileMan->getOutputFile(theInputManager.specificFileName);
  else if (extFlag == false && ext_sigma_flag==false)
    outFile = fileMan->getOutputFile();
  else if (extFlag == true && ext_sigma_flag==false){
    CFD_scale_factor = CFD_scale_factor/10.0; //bash script does things in whole numbers
    outFile = fileMan->getOutputFile(FL,FG,CFD_delay,CFD_scale_factor*10);
  } else if (extFlag==false && ext_sigma_flag==true){
    sigma=sigma/10;
    outFile= fileMan->getOutputFile(sigma*10);
  }

  outT = new TTree("flt","Filtered Data Tree --- Comment Description");
  cout << "Creating filtered Tree"<<endl;
  if(!outT)
    {
      cout << "\nCould not create flt Tree in " << fileMan->outputFileName.str() << endl;
      exit(-1);
    }
  ////////////////////////////////////////////////////////////////////////////////////
  
  Int_t numOfChannels=4;  
  // set input tree branvh variables and addresses
  ////////////////////////////////////////////////////////////////////////////////////
  Int_t chanid;
  Int_t slotid;
  vector<UShort_t> trace;
  UInt_t fUniqueID;
  UInt_t energy;
  Double_t time ; 
  UInt_t timelow; // this used to be usgined long
  UInt_t timehigh; // this used to be usgined long
  UInt_t timecfd ; 
  Double_t correlatedTimes_in[numOfChannels];
  Double_t integrals_in[numOfChannels];
  if (! correctionRun ){
    //In put tree branches    
    inT->SetBranchAddress("chanid", &chanid);
    inT->SetBranchAddress("fUniqueID", &fUniqueID);
    inT->SetBranchAddress("energy", &energy);
    inT->SetBranchAddress("timelow", &timelow);
    inT->SetBranchAddress("timehigh", &timehigh);
    inT->SetBranchAddress("trace", &trace);
    inT->SetBranchAddress("timecfd", &timecfd);
    inT->SetBranchAddress("slotid", &slotid);
    inT->SetBranchAddress("time", &time);
  } else {
    inT->SetBranchAddress("Time0",&correlatedTimes_in[0]);
    inT->SetBranchAddress("Time1",&correlatedTimes_in[1]);
    inT->SetBranchAddress("Time2",&correlatedTimes_in[2]);
    inT->SetBranchAddress("Time3",&correlatedTimes_in[3]);

    inT->SetBranchAddress("Integral0",&integrals_in[0]);
    inT->SetBranchAddress("Integral1",&integrals_in[1]);
    inT->SetBranchAddress("Integral2",&integrals_in[2]);
    inT->SetBranchAddress("Integral3",&integrals_in[3]);

  }
  
  ////////////////////////////////////////////////////////////////////////////////////


  //set output tree branches and varibables 
  ////////////////////////////////////////////////////////////////////////////////////


  vector <Sl_Event> previousEvents;
  Double_t sizeOfRollingWindow=4;
  
  //Out put tree branches 
  Double_t timeDiff,timeDiff1,timeDiff2,timeDiff3,timeDiff4; 
  
  Double_t timeDiffgoecor1,timeDiffgoecor2,timeDiffcor12;
  Double_t timeDiffdtcor1,timeDiffdtcor2;

  
  outT->Branch("Time_Diff",&timeDiff,"Time_Diff/D");

  outT->Branch("Time_Diff1",&timeDiff1,"Time_Diff1/D");
  outT->Branch("Time_Diff2",&timeDiff2,"Time_Diff2/D");
  outT->Branch("Time_Diff3",&timeDiff3,"Time_Diff3/D");
  outT->Branch("Time_Diff4",&timeDiff4,"Time_Diff4/D");

  outT->Branch("Time_Diffgoecor1",&timeDiffgoecor1,"Time_Diffgoecor1/D");
  outT->Branch("Time_Diffdtcor1",&timeDiffdtcor1,"Time_Diffdtcor1/D");

  outT->Branch("Time_Diffdtcor2",&timeDiffdtcor2,"Time_Diffdtcor2");
  outT->Branch("Time_Diffgoecor2",&timeDiffgoecor2,"Time_Diffgoecor2/D");

  outT->Branch("Time_Diffcor12",&timeDiffcor12,"Time_Diffcor12/D");
  
  Double_t timeDiffRaw;
  outT->Branch("Time_Diff_Raw",&timeDiffRaw,"Time_Diff_Raw/D");

  Double_t softwareCFD;
  outT->Branch("SoftwareCFD",&softwareCFD,"SoftwareCDF/D");

  Double_t integrals[numOfChannels];
  Double_t integrals_cor[numOfChannels];
    
  
  outT->Branch("Integral0",&integrals[0],"Integral0/D");
  outT->Branch("Integral1",&integrals[1],"Integral1/D");
  outT->Branch("Integral2",&integrals[2],"Integral2/D");
  outT->Branch("Integral3",&integrals[3],"Integral3/D");

  outT->Branch("Integral0_cor",&integrals_cor[0],"Integral0_cor/D");
  outT->Branch("Integral1_cor",&integrals_cor[1],"Integral1_cor/D");
  outT->Branch("Integral2_cor",&integrals_cor[2],"Integral2_cor/D");
  outT->Branch("Integral3_cor",&integrals_cor[3],"Integral3_cor/D");

  Double_t correlatedTimes[numOfChannels];
  outT->Branch("Time0",&correlatedTimes[0],"Time0/D");
  outT->Branch("Time1",&correlatedTimes[1],"Time1/D");
  outT->Branch("Time2",&correlatedTimes[2],"Time2/D");
  outT->Branch("Time3",&correlatedTimes[3],"Time3/D");



  Double_t delta_T1;
  outT->Branch("Delta_T1",&delta_T1,"Delta_T1/D");
  Double_t delta_T2;
  outT->Branch("Delta_T2",&delta_T2,"Delta_T2/D");
  
  Double_t GravityOfEnergy1;
  outT->Branch("GravityOfEnergy1",&GravityOfEnergy1,"GravityOfEnergy1/D");

  Double_t GravityOfEnergy2;
  outT->Branch("GravityOfEnergy2",&GravityOfEnergy2,"GravityOfEnergy2/D");

  Double_t GOE1;
  Double_t GOE2;

  outT->Branch("GOE1",&GOE1,"GOE1/D");
  outT->Branch("GOE2",&GOE2,"GOE2/D");


  //Branches for explict trace reconstruction
  TH2F *traces   = new TH2F("traces","This these are the original traces",200,0,200,10000,-1000,1000);
  TH2F *filters = new TH2F("filters","The filters",200,0,200,10000,-1000,4000);
  TH2F *CFDs  = new TH2F("CFDs","The CFDs",200,0,200,10000,-1000,1000);

  TGraph * traces2 = new TGraph(200);

  if (makeTraces) //adding the branches to the tree slows things down   
    {             //so only do it if you really want them
      outT->Branch("Traces","TH2F",&traces,128000,0);  
      outT->Branch("Filters","TH2F",&filters,12800,0);
      outT->Branch("CFDs","TH2F",&CFDs,12800,0);
      outT->Branch("Traces2","TGraph",&traces2,128000,0);
    }
  Double_t eventNum;
  outT->Branch("Jentry",&eventNum,"Jengrty/D");
  
  Double_t eventTriggerNum;
  outT->Branch("EventTriggerNum",&eventTriggerNum,"EventTriggerNum/D");
  ////////////////////////////////////////////////////////////////////////////////////

  if(maxentry == -1)
    maxentry=nentry;
  
  if (makeTraces)
    maxentry=50;//cap off the number of entries for explict trace making

  //non branch timing variables 
  ////////////////////////////////////////////////////////////////////////////////////
  Double_t prevTime =0;
  vector <Double_t> thisEventsFF;
  vector <Double_t> thisEventsCFD;
  //zero crossings
  Double_t thisEventsIntegral=0;
  Filter theFilter; // Filter object
  ////////////////////////////////////////////////////////////////////////////////////
  Bool_t normalSet=true;
  for (Long64_t jentry=0; jentry<10;jentry++){
    inT->GetEntry(jentry);
    if(slotid!=2){
      normalSet=false;
      break;
    }
  }


  for (Long64_t jentry=0; jentry<maxentry;jentry++) { // Main analysis loop
    
    inT->GetEntry(jentry); // Get the event from the input tree 
    eventNum=jentry;
    if (normalSet == false)
      chanid=slotid;
    
    //initializations for branch variables
    ///////////////////////////////////////////////////////////////////////////////////////////
    eventTriggerNum=0;//its 0 when there is no correlated event found on this loop
    //set to one below if there was a correlated event fround in the 
    //previous set of events (sizeOfRollingWindow)
    timeDiffRaw=0;     //TimeDiffRaw is just the difference between the previous event and
                       //the current one
    timeDiff = BAD_NUM;  //make it something random to distinguish uncorrleated events  
    softwareCFD = BAD_NUM;//ditto
    
    GravityOfEnergy1 = BAD_NUM;    
    GravityOfEnergy2 = BAD_NUM;    
    GOE1=BAD_NUM;
    GOE2=BAD_NUM;
    delta_T1 = BAD_NUM;
    delta_T2 = BAD_NUM;
    
    for (Int_t i=0;i<(Int_t) numOfChannels;++i){
      integrals[i]=BAD_NUM; //ditto
      integrals_cor[i]=BAD_NUM;
    }
    //software genearted filters
    thisEventsCFD.clear();//Clear the CFD vector 
    thisEventsFF.clear();//Clear this events fast filter
    
    if (makeTraces) //reset histograms if makeTraces is on
      {
	traces->Reset();
	filters->Reset();
	CFDs->Reset();
      }
    
    ///////////////////////////////////////////////////////////////////////////////////////////


    
    //Time_diff raw 
    timeDiffRaw = time - prevTime;
    prevTime = time;
    ///
    if((theInputManager.timingMode == "softwareCFD" || theInputManager.timingMode == "fitting") && chanid < 14){
      if(theInputManager.timingMode == "fitting" ){
	softwareCFD = theFilter.fitTrace(trace,sigma,jentry);
      } else {
	theFilter.FastFilter(trace,thisEventsFF,FL,FG);
	//theFilter.FastFilterFull(trace,thisEventsFF,FL,FG,40);
	if (makeTraces )	{
	  for (int i=0;i< (int) trace.size();i++) {
	    traces->Fill(i,trace[i]);	
	    traces2->SetPoint(i,i,trace[i]);
	    filters->Fill(i, thisEventsFF[i]);
	  }
	}
        thisEventsCFD = theFilter.CFD(thisEventsFF,CFD_delay,CFD_scale_factor);
	softwareCFD = theFilter.GetZeroCrossing(thisEventsCFD);
	if (makeTraces){
	  for (Int_t j=0;j<(Int_t) thisEventsCFD.size();++j)
	    CFDs->Fill(j,thisEventsCFD[j]);
	}
      }
    }

    if (!correctionRun && trace.size()!=0)
      thisEventsIntegral = theFilter.getEnergy(trace);
    else if (trace.size() == 0) {
      if (energy ==0)
	thisEventsIntegral=BAD_NUM;
      else
	thisEventsIntegral=energy;
    }
    
    if ( previousEvents.size() >= sizeOfRollingWindow )
      {
	if ( checkChannels(previousEvents) )
	  { // there are all four channels



	    //for cable arrangement independance
	    vector <Double_t> times_extra(20,0);
	    vector <Double_t> times; // there are only 4 channels
	    vector <Double_t> integrals_extra(20,0);
	    vector <Double_t> integrals_ordered;
	    vector <Double_t> times_cor(4,0);
	    
	    for (Int_t i=0;i<previousEvents.size();i++){

	      times_extra[previousEvents[i].channel]=previousEvents[i].time;
	      integrals_extra[previousEvents[i].channel]=previousEvents[i].integral;
	    }

	    for (int i=0;i<times_extra.size();i++){
	      if (times_extra[i]!=0 && integrals_extra[i]!=0){
		times.push_back(times_extra[i]);
		integrals_ordered.push_back(integrals_extra[i]);//can't use branch
	      }//variable here because it has to already be defined and set to a size
	    }
	    
	    

	    if (TMath::Abs ( (times[0]+times[1]-times[2]-times[3])/2 ) <10.0){
	      if ( times[0] != BAD_NUM &&  times[1] != BAD_NUM && times[2] != BAD_NUM&& times[3] != BAD_NUM){
		//Good Event

		times_cor[1]=times[1]-SDelta_T1_Cor;
		times_cor[3]=times[3]-SDelta_T2_Cor;
		times_cor[0]=times[0];
		times_cor[2]=times[2];
		delta_T1 =  times_cor[1]-times_cor[0];
		delta_T2 =  times_cor[3]-times_cor[2];


		timeDiff = (times_cor[0]+times_cor[1]-times_cor[2]-times_cor[3])/2 +10;
		timeDiff1 = (times_cor[0]-times_cor[2]) +10;
		timeDiff2 = (times_cor[1]-times_cor[3]) +10;
		timeDiff3 = (times_cor[0]-times_cor[3]) +10;
		timeDiff4 = (times_cor[1]-times_cor[2]) +10;	      
		//save times for later analyses with corrections
		for (int q=0;q<numOfChannels;q++)
		  correlatedTimes[q]=times[q];

		
		
		for (int q=0;q<integrals_ordered.size();q++){
		  integrals[q]=integrals_ordered[q];//assign the values to the branch var
		  //since the channels are ordered.  the integrals[0] will always bee the same channel
		  //so I don't need to check;
		  integrals_cor[q]=integrals[q]*int_corrections[q];
		}
		
		GravityOfEnergy1 = (integrals[1]-integrals[0])/(integrals[0]+integrals[1]);
		GravityOfEnergy2 = (integrals[3]-integrals[2])/(integrals[2]+integrals[3]);
		GOE1 = (integrals_cor[1]-integrals_cor[0])/(integrals_cor[0]+integrals_cor[1]);
		GOE2 = (integrals_cor[3]-integrals_cor[2])/(integrals_cor[2]+integrals_cor[3]);

		vector <Double_t> tot(4,0); //there are 2 GOE corrections and 2 dt corrections
		for (int q=1;q<=degree;q++){
		  if (TMath::Abs(GOE1)<1)
		    tot[0]=tot[0]+GOE_cor1[q-1]*TMath::Power(GOE1,q);
		  if (TMath::Abs(GOE2)<1)
                    tot[1]=tot[1]+GOE_cor2[q-1]*TMath::Power(GOE2,q);
		  if (TMath::Abs(delta_T1)<1)
                    tot[2]=tot[2]+DeltaT_cor1[q-1]*TMath::Power(delta_T1,q);
		  if (TMath::Abs(delta_T2)<1)
                    tot[3]=tot[3]+DeltaT_cor2[q-1]*TMath::Power(delta_T2,q);
		}
		timeDiffgoecor1=timeDiff-tot[0];
		timeDiffgoecor2=timeDiff-tot[1]-tot[0];
		timeDiffdtcor1=timeDiff-tot[2];
		timeDiffdtcor2=timeDiff-tot[3]-tot[2];

		/*		timeDiffgoecor1 = timeDiff - GOE1*GOE_cor1;
		timeDiffdtcor1 = timeDiff - delta_T1*Delta_T1_Cor_0 -delta_T1*delta_T1*Delta_T1_Cor_1 -TMath::Power(delta_T1,3)*Delta_T1_Cor_2;

		timeDiffdtcor2 = timeDiff -delta_T2*Delta_T2_Cor;

		timeDiffgoecor2 =timeDiff -GOE2*GOE_cor2-GOE1*GOE_cor1;
		*/
		//	        timeDiffcor12 =timeDiff - GravityOfEnergy2*GOE_cor2 - GravityOfEnergy1*GOE_cor1;
	      
		//		timeDiffcor1 = timeDiff - GravityOfEnergy1*(0.113013);
		//
		//	timeDiffcor12 =timeDiff - GravityOfEnergy2*(-0.0449815) - GravityOfEnergy1*(0.113013);
		// -0.0449815 correcions determinded on GOE2:time_diffcor1
		if(! makeTraces){
		  outT->Fill();

		}
	      }
	    }
	  }
      }
    //over write the time when in trace fitting mode or software CFD mode
    if (theInputManager.timingMode == "softwareCFD" || theInputManager.timingMode == "fitting")
      time = softwareCFD ;
    Int_t loop=1;
    if (correctionRun)
      loop=numOfChannels;

    for(int l=0;l<loop;l++){
      if (correctionRun){
	chanid=l;//the Events were ordered in the previous building
	thisEventsIntegral=integrals_in[l];
	time=correlatedTimes_in[l];
      }
      
      //Keep the previous event info for correlating      
      if (previousEvents.size() < sizeOfRollingWindow  ) 
	{
	  Sl_Event e;
	  e.channel=chanid;
	  e.time = time;
	  e.integral=thisEventsIntegral;
	  e.softwareCFD = softwareCFD;
	  previousEvents.push_back(e);
	}
      else if (previousEvents.size() >= sizeOfRollingWindow )
	{
	  //So starting on the fith element 
	  previousEvents.erase(previousEvents.begin(),previousEvents.begin() + 1);
	  Sl_Event e;
	  e.channel=chanid;
	  e.time=time;
	  e.integral=thisEventsIntegral;
	  e.softwareCFD = softwareCFD;
	  previousEvents.push_back(e);	  
	}
    }
    //Periodic printing
    if (jentry % 10000 == 0 )
      cout<<"On event "<<jentry<<endl;

    //Fill the tree
    if (makeTraces)
      outT->Fill();
    
  }//End for

  
  
  outT->Write();
  outFile->Close();

  cout<<"Number of bad fits "<<theFilter.numOfBadFits<<endl;

  cout<<"\n\n**Finished**\n\n";

  return  0;

}

