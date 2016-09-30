#include "Plotter.h"

using namespace std;

#define turnLatex(x) ((latexer[x] == "") ? x : latexer[x])

unordered_map<string, string> Plotter::latexer = { {"GenTau", "#tau"}, {"GenHadTau", "#tau_h"}, {"GenMuon", "#mu"}, {"TauJet", "#tau"}, {"Muon", "#mu"}, {"DiMuon", "#mu, #mu"}, {"DiTau", "#tau, #tau"}, {"Tau", "#tau"}, {"DiJet", "jj"}, {"Met", "#slash{E}_{T}"}, {"BJet", "b"}};

template <typename T>
string to_string_with_precision(const T a_value, const int n = 6);

void Plotter::CreateStack( TDirectory *target, Logfile& logfile) {

  gStyle = styler.getStyle();

  TList* datalist = FileList[0];
  TList* bglist = FileList[1];
  TList* sglist = FileList[2];

  bool noData = datalist->GetSize() == 0;

  if(bglist->GetSize() == 0) {
    cout << "No backgrounds given: Aborting" << endl;
    exit(1);
  }
  if(noData) cout << "No Data given: Plotting without Data" << endl;

  TString path( (char*)strstr( target->GetPath(), ":" ) );
  path.Remove( 0, 2 );

  TFile *firstFile = (TFile*)bglist->First();
  firstFile->cd( path );
  TDirectory *current_sourcedir = gDirectory;
  Bool_t status = TH1::AddDirectoryStatus();
  TH1::AddDirectory(kFALSE);


  ///try to find events to calculate efficiency
  TH1* events;
  current_sourcedir->GetObject("Events", events);

  if(events) {
    vector<string> logEff;
    string totalval = "";
    logEff.push_back(current_sourcedir->GetName());

    for(int i=0; i < 3; i++) {
      TFile* nextsource = (TFile*)FileList[i]->First();
      while ( nextsource ) {
	nextsource->cd(path);
	gDirectory->GetObject("Events", events);
	totalval = to_string_with_precision(events->GetBinContent(2), 1);
	if(i != 0) totalval += " $\\pm$ " + to_string_with_precision(events->GetBinError(2), 1);
	logEff.push_back(totalval);
	nextsource = (TFile*)FileList[i]->After( nextsource );
      }
    }
    logfile.addLine(logEff);
    delete events;
  }
  events = NULL;


  // loop over all keys in this directory
  TIter nextkey( current_sourcedir->GetListOfKeys() );
  TKey *key, *oldkey=0;
  while ( (key = (TKey*)nextkey())) {

    //keep only the highest cycle number for each key
    if (oldkey && !strcmp(oldkey->GetName(),key->GetName())) continue;

    //   firstFile->cd( path );

    TObject *obj = key->ReadObj();
    if ( obj->IsA() ==  TH1D::Class() || obj->IsA() ==  TH1F::Class() ) {
      
      TH1 *h1 = (TH1*)obj;
      TH1D* error = new TH1D("error", h1->GetTitle(), h1->GetXaxis()->GetNbins(), h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());
      TH1D* datahist = new TH1D("data", h1->GetTitle(), h1->GetXaxis()->GetNbins(), h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());
      TList* sigHists = new TList();
      THStack *hs = new THStack(h1->GetName(),h1->GetName());
      
      /*------------data--------------*/

      //////hybrid

      int nfile = 0;

      for(int i = 0; i < 3; i++) {
	TFile* nextfile = (TFile*)FileList[i]->First();
	while ( nextfile ) {
	  nextfile->cd( path );
	  TKey *key2 = (TKey*)gDirectory->GetListOfKeys()->FindObject(h1->GetName());
	  if(key2) {
	    TH1* h2 = (TH1*)key2->ReadObj();
     /*------------Data--------------*/
	    if(i == 0)  datahist->Add(h2);
	    else if(i == 1) {
     /*------------background--------------*/
	      error->Add(h2);
	      for(int j = 1; j < h2->GetXaxis()->GetNbins()+1; j++) {
		h2->SetBinError(j, 0);
	      }
	      //////style
	      string title = nextfile->GetTitle();
	      title = title.substr(0, title.size()-5);
	      h2->SetTitle(title.c_str());
	      h2->SetLineColor(1);
	      h2->SetFillStyle(1001);
	      h2->SetFillColor(color[nfile]);

	      hs->Add(h2);
	      nfile++;
	    } else if(i == 2) {
     /*------------Signal--------------*/
	      for(int j = 1; j < h2->GetXaxis()->GetNbins()+1; j++) {
		h2->SetBinError(j, 0);
	      }

	      //////style
	      string title = nextfile->GetTitle();
	      title = title.substr(0, title.size()-5);
	      h2->SetTitle(title.c_str());
	      h2->SetLineColor(color[nfile]);
	      h2->SetLineWidth(3);
	      h2->SetLineStyle(2);

	      sigHists->Add(h2);
	      nfile++;
	    }
	    //	    delete h2;
	  }
	  nextfile = (TFile*)FileList[i]->After(nextfile);
	}
      }
      
    
      /*--------------write out------------*/


      datahist->SetMarkerStyle(20);
      datahist->SetLineColor(1);

      hs = sortStack(hs);

      ///rebin
      ////////////decide how to rebin(data, background, both)
      ////check if binning is valid
      vector<double> bins;
      if(noData ) bins = rebinner(error, styler.getRebinLimit());
      else bins = rebinner(datahist, styler.getRebinLimit());


      ////get rid of continue, need things to delete!
      if(bins.size() == 0) continue;


      double* binner = new double[bins.size()];
      bool passed = true;
      binner[0] = bins.back();
      for(int i = 1; i < bins.size(); i++) {
      	if(bins.at(bins.size() - i) >= bins.at(bins.size() - i - 1))  {
      	  passed = false;
      	  break;
      	}
      	binner[i] = bins.at(bins.size() - i - 1);
      }



      ////rebin histograms
      THStack* hsdraw = hs;
      if(passed && bins.size() > styler.getBinLimit()) {
	datahist = (TH1D*)datahist->Rebin(bins.size()-1, "data_rebin", binner);
      	error = (TH1D*)error->Rebin(bins.size()-1, "error_rebin", binner);
      	hsdraw = rebinStack(hs, binner, bins.size()-1);	
      	TList* tmplist = new TList();
      	TH1D* onesig = (TH1D*)sigHists->First();
      	while(onesig) {
      	  tmplist->Add(onesig->Rebin(bins.size()-1, onesig->GetName(), binner));
      	  onesig = (TH1D*)sigHists->After(onesig);
      	}
      	delete sigHists;
      	sigHists = tmplist;
      } 



      ///legend stuff
      TLegend* legend = createLeg(datahist, hsdraw->GetHists(), sigHists);
      
      ////divide by binwidth is option is given
      if(styler.getDivideBins()) divideBin(datahist, error, hsdraw, sigHists); ///add sig stuff as well
      
      //error
      TGraphErrors* errorstack = createError(error, false);

      ////draw graph
      target->cd();

      TCanvas *c = new TCanvas(h1->GetName(), h1->GetName());//403,50,600,600);
      // TPaveText* text = new TPaveText(0.05, 0.7, 0.5, 1.);
      // text->AddText("CMS Preliminary");
      // text->Draw();

      if(!(noData || onlyTop)) {
	c->Divide(1,2);
	c->cd(1);
	sizePad(styler.getPadRatio(), gPad, true);
      }

      hsdraw->Draw();
      datahist->Draw("same");

      TH1D* tmpsig = (TH1D*)sigHists->First();
      while(tmpsig) {
	tmpsig->Draw("same");
	tmpsig = (TH1D*)sigHists->After(tmpsig);
      }
      errorstack->Draw("2");
      legend->Draw();
      setYAxisTop(datahist, error, styler.getHeightRatio(), hsdraw);
      if(onlyTop || noData) {
	hsdraw->GetXaxis()->SetTitle(newLabel(hsdraw->GetTitle()).c_str());     
	hsdraw->GetXaxis()->SetTitleSize(hsdraw->GetYaxis()->GetLabelSize());
      }



      // ///second pad
      TF1* PrevFitTMP = NULL;
      TGraphErrors* errorratio = NULL;
      TList* signalBot = NULL;

      if( !(noData || onlyTop) ) {
	c->cd(2);
	sizePad(styler.getPadRatio(), gPad, false);

	TH1* botaxis = error;
	botaxis->Draw("AXIS");
	setXAxisBot(botaxis, styler.getPadRatio());

	signalBot = (bottomType != Ratio) ? signalBottom(sigHists, error) : signalBottom(sigHists, datahist, error);
	
	errorratio = createError(error, true);
	if(bottomType == Ratio) {
	  tmpsig = (TH1D*)signalBot->Last();
	  PrevFitTMP = createLine(tmpsig);
	  setYAxisBot(error->GetYaxis(), tmpsig, styler.getPadRatio());
	} else setYAxisBot(botaxis->GetYaxis(), signalBot, styler.getPadRatio());

	tmpsig = (TH1D*)signalBot->First();
	while(tmpsig) {
	  tmpsig->Draw("same");
	  tmpsig = (TH1D*)signalBot->After(tmpsig);
	}
	if(bottomType == Ratio) errorratio->Draw("2");
      }

      c->cd();
      c->Write(c->GetName());
      c->Close();
      
      hsdraw->Delete();
      delete datahist;
      delete error;
      delete sigHists;
      delete legend;
      delete errorstack;

      delete[] binner;
      if( !(noData || onlyTop) ) {
	delete errorratio;
	delete PrevFitTMP;
	signalBot->Delete();
      }

    } else if ( obj->IsA()->InheritsFrom( TDirectory::Class() ) ) {
      target->cd();
      TDirectory *newdir = target->mkdir( obj->GetName(), obj->GetTitle() );

      CreateStack( newdir, logfile );

    } else if ( obj->IsA()->InheritsFrom( TH1::Class() ) ) {
      continue;

    } else {
         cout << "Unknown object type, name: "
	   << obj->GetName() << " title: " << obj->GetTitle() << endl;
    }
  }

  TH1::AddDirectory(status);
}



template <typename T>
string to_string_with_precision(const T a_value, const int n)
{
  ostringstream out;
  out << fixed << setprecision(n) << a_value;
  return out.str();
}




THStack* Plotter::sortStack(THStack* old) {
  if(old == NULL || old->GetNhists() == 0) return old;
  string name = old->GetName();
  THStack* newstack = new THStack(name.c_str(),name.c_str());

  TList* list = (TList*)old->GetHists();

  while(list->GetSize() > 1) {

    TH1* smallest = (TH1*)list->First();;
    TH1* tmp = (TH1*)list->After(smallest);
    while ( tmp ) {
      if(smallest->Integral() > tmp->Integral()) smallest = tmp;
      tmp = (TH1*)list->After(tmp);
    }
    newstack->Add(smallest);
    list->Remove(smallest);
  }
  TH1* last = (TH1*)list->First();
  if(last) newstack->Add(last);
  
  delete old;
  return newstack;
} 


////make legend position adjustable
TLegend* Plotter::createLeg(const TH1* data, const TList* bgl, const TList* sigl) {
  double width = 0.04;
  int items = bgl->GetSize() + sigl->GetSize();
  items += (data->GetEntries() != 0) ? 1 : 0;
  TLegend* leg = new TLegend(0.73, 0.9-items*width ,0.93,0.90);
  
  if(data->GetEntries() != 0) leg->AddEntry(data, "Data", "lep");
  TH1* tmp = (TH1*)bgl->First();
  while(tmp) {
    leg->AddEntry(tmp, tmp->GetTitle(), "f");
    tmp = (TH1*)bgl->After(tmp);
  }
  tmp = (TH1*)sigl->First();
  while(tmp) {
    leg->AddEntry(tmp, tmp->GetTitle(), "lep");
    tmp = (TH1*)sigl->After(tmp);
  }

  return leg;
}

TGraphErrors* Plotter::createError(const TH1* error, bool ratio) {
  int Nbins =  error->GetXaxis()->GetNbins();
  Double_t* mcX = new Double_t[Nbins];
  Double_t* mcY = new Double_t[Nbins];
  Double_t* mcErrorX = new Double_t[Nbins];
  Double_t* mcErrorY = new Double_t[Nbins];

  for(int bin=0; bin < error->GetXaxis()->GetNbins(); bin++) {
    mcY[bin] = (ratio) ? 1.0 : error->GetBinContent(bin+1);
    mcErrorY[bin] = (ratio) ?  error->GetBinError(bin+1)/error->GetBinContent(bin+1) : error->GetBinError(bin+1);
    mcX[bin] = error->GetBinCenter(bin+1);
    mcErrorX[bin] = error->GetBinWidth(bin+1) * 0.5;
  }
  TGraphErrors *mcError = new TGraphErrors(error->GetXaxis()->GetNbins(),mcX,mcY,mcErrorX,mcErrorY);

  mcError->SetLineWidth(1);
  mcError->SetFillColor(1);
  mcError->SetFillStyle(3002);

  delete[] mcX;
  delete[] mcY;
  delete[] mcErrorX;
  delete[] mcErrorY;
  return mcError;
}

void Plotter::sizePad(double ratio, TVirtualPad* pad, bool isTop) {
  if(isTop) pad->SetPad("top", "top", 0, 1 / (1.0 + ratio), 1, 1, 0);
  else  {
    pad->SetPad("bottom", "bottom", 0, 0, 1, 1 / (1.0 + ratio), 0);
    pad->SetMargin(pad->GetLeftMargin(),pad->GetRightMargin(),ratio*pad->GetBottomMargin(),0);
    pad->SetTitle("");
  }
}

TF1* Plotter::createLine(TH1* data_mc) {
  TF1 *PrevFitTMP = new TF1("PrevFitTMP","pol0",-10000,10000);
  PrevFitTMP->SetMarkerStyle(20);
  PrevFitTMP->SetLineColor(2);
  PrevFitTMP->SetLineWidth(1);
  PrevFitTMP->SetParameter(0,1.0);
  PrevFitTMP->SetParError(0,0);
  PrevFitTMP->SetParLimits(0,0,0);
  data_mc->GetListOfFunctions()->Add(PrevFitTMP);
  return PrevFitTMP;
}
 
vector<double> Plotter::rebinner(const TH1* hist, double limit) {
  vector<double> bins;
  double toterror = 0.0, prevbin=0.0;
  double limit2 = pow(limit,2);
  bool foundfirst = false;
  double end;

  //how to tell if ok?

  if(hist->GetEntries() == 0 || hist->Integral() <= 0) return bins;

   for(int i = hist->GetXaxis()->GetNbins(); i > 0; i--) {
    if(hist->GetBinContent(i) <= 0.0) continue;
    if(!foundfirst) { 
      bins.push_back(hist->GetXaxis()->GetBinUpEdge(i));
      foundfirst = true;
    } else end = hist->GetXaxis()->GetBinLowEdge(i);

    if(toterror* prevbin != 0.) toterror *= pow(prevbin,2)/pow(prevbin+hist->GetBinContent(i),2);
    prevbin += hist->GetBinContent(i);
    toterror += (2 * pow(hist->GetBinError(i),2))/pow(prevbin,2);
    if(toterror < limit2) {
      bins.push_back(hist->GetXaxis()->GetBinLowEdge(i));
      toterror = 0.0;
      prevbin = 0.0;
    }
  }

  if(bins.back() != end) {
    bins.push_back(end);
    if(hist->GetXaxis()->GetXmin() >= 0 && end !=hist->GetXaxis()->GetXmin()) bins.push_back(hist->GetXaxis()->GetXmin());
  }

  return bins;
}


THStack* Plotter::rebinStack(THStack* hs, const double* binner, int total) {
  THStack* newstack = new THStack(hs->GetName(), hs->GetName());
  TList* list = (TList*)hs->GetHists();

  TH1D* tmp = (TH1D*)list->First();
  while ( tmp ) {
    TH1D* forstack = (TH1D*)tmp->Clone();
    forstack = (TH1D*)forstack->Rebin(total, tmp->GetName(), binner);
    newstack->Add(forstack);
    tmp = (TH1D*)list->After(tmp);
  }
  
  hs->Delete();

  return newstack;
}



void Plotter::setYAxisTop(TH1* datahist, TH1* error, double ratio, THStack* hs) {
  TAxis* yaxis = hs->GetYaxis();
  if(styler.getDivideBins()) yaxis->SetTitle("Events/GeV");////get axis title stuff
  else yaxis->SetTitle("Events");
  ///  yaxis->SetLabelSize(hs->GetXaxis()->GetLabelSize());
  double max = (error->GetMaximum() > datahist->GetMaximum()) ? error->GetMaximum() : datahist->GetMaximum();

   hs->SetMaximum(max*(1.0/ratio + 1.0));

}


TList* Plotter::signalBottom(const TList* signal, const TH1D* background) {
  TList* returnList = new TList();
  
  TH1D* holder = (TH1D*)signal->First();

  while(holder) {
    TH1D* signif = (TH1D*)holder->Clone();
    int Nbins = signif->GetXaxis()->GetNbins();
    for(int i = 0; i < Nbins;i++) {
      if(signif->GetBinContent(i+1) <= 0 && background->GetBinContent(i+1) <= 0) continue;
      int edge1 = i+1, edge2= i+1;
      double sigErr, backErr;
      if(bottomType == SigLeft) edge1 = 0;
      if(bottomType == SigRight) edge2 = Nbins;
      double sigInt = signif->IntegralAndError(edge1, edge2, sigErr);
      double backInt = background->IntegralAndError(edge1, edge2, backErr);
	  
      double total = (ssqrtsb) ? sigInt/sqrt(sigInt+backInt) : sigInt/sqrt(backInt);
      double perErr = (ssqrtsb) ? pow(sigErr/sigInt-sigErr/(2*(sigInt+backInt)),2) + pow(backErr/(2*(sigInt+backInt)),2) : pow(sigErr/sigInt,2) + pow(backErr/(2*backInt),2);

      signif->SetBinContent(i+1, total);
      signif->SetBinError(i+1, total*perErr);
    }
    returnList->Add(signif);
    holder = (TH1D*)signal->After(holder);
  }
  holder = NULL;
  return returnList;
}

TList* Plotter::signalBottom(const TList* signal, const TH1D* data, const TH1D* background) {
  TList* returnList = new TList();
  
  TH1D* holder = (TH1D*)signal->First();

  while(holder) {
    TH1D* total = (TH1D*)holder->Clone();
    total->Add(background);
    
    total->Divide(data, total);

    returnList->Add(total);
    holder = (TH1D*)signal->After(holder);
  }
  TH1D* data_mc = (TH1D*)data->Clone();
  data_mc->Divide(background);
  returnList->Add(data_mc);

  holder = NULL;
  return returnList;
}

  

void Plotter::setXAxisBot(TH1* data_mc, double ratio) {
  TAxis* xaxis = data_mc->GetXaxis();
  xaxis->SetTitle(newLabel(data_mc->GetTitle()).c_str());
  xaxis->SetLabelSize(xaxis->GetLabelSize()*ratio);
}


void Plotter::setYAxisBot(TAxis* yaxis, TH1* data_mc, double ratio) {
  double divmin = 0.0, divmax = 2.99;
  double low=2.99, high=0.0, tmpval;
  for(int i = 0; i < data_mc->GetXaxis()->GetNbins(); i++) {
    tmpval = data_mc->GetBinContent(i+1);
    if(tmpval < 2.99 && tmpval > high) {high = tmpval;}
    if(tmpval > 0. && tmpval < low) {low = tmpval;}
  }
  double val = min(abs(1 / (high - 1.)), abs(1 / (1/low -1.)));
  if(high == 0.0) val = 0;
  double factor = 2.0;
  while(val > factor || (factor == 2 && val > 1.)) {
    divmin = 1.0 - 1.0/factor;
    divmax = 1/divmin;
    factor *= 2.0;
  }

  yaxis->SetRangeUser(divmin - 0.00001,divmax - 0.00001);
  yaxis->SetLabelSize(yaxis->GetLabelSize()*ratio);
  yaxis->SetTitleSize(ratio*yaxis->GetTitleSize());
  yaxis->SetTitleOffset(yaxis->GetTitleOffset()/ratio);
  yaxis->SetTitle("#frac{Data}{MC}");
}

void Plotter::setYAxisBot(TAxis* yaxis, TList* signal, double ratio) {
  double max = 0;
  TH1D* tmphist = (TH1D*)signal->First();
  while(tmphist) {
    max = (max < tmphist->GetMaximum()) ? tmphist->GetMaximum() : max;
    tmphist = (TH1D*)signal->After(tmphist);
  }

  yaxis->SetRangeUser(0,max*11./10 -0.00001);
  yaxis->SetLabelSize(yaxis->GetLabelSize()*ratio);
  yaxis->SetTitleSize(ratio*yaxis->GetTitleSize());
  yaxis->SetTitleOffset(yaxis->GetTitleOffset()/ratio);
  if(ssqrtsb) yaxis->SetTitle("#frac{S}{#sqrt{S+B}}");
  else  yaxis->SetTitle("#frac{S}{#sqrt{B}}");
}


void Plotter::divideBin(TH1* data, TH1* error, THStack* hs, TList* signal) {

  for(int i = 0; i < data->GetXaxis()->GetNbins(); i++) {
    data->SetBinContent(i+1, data->GetBinContent(i+1)/data->GetBinWidth(i+1));
    data->SetBinError(i+1, data->GetBinError(i+1)/data->GetBinWidth(i+1));
  }
  for(int i = 0; i < error->GetXaxis()->GetNbins(); i++) {
    error->SetBinContent(i+1, error->GetBinContent(i+1)/error->GetBinWidth(i+1));
    error->SetBinError(i+1, error->GetBinError(i+1)/error->GetBinWidth(i+1));
  }

  TList* list = (TList*)hs->GetHists();

  TIter next(list);
  TH1* tmp = NULL;
  while ( (tmp = (TH1*)next()) ) {
    for(int i = 0; i < tmp->GetXaxis()->GetNbins(); i++) {
      tmp->SetBinContent(i+1,tmp->GetBinContent(i+1)/tmp->GetBinWidth(i+1));
      tmp->SetBinError(i+1,tmp->GetBinError(i+1)/tmp->GetBinWidth(i+1));
    }
  }
  tmp = (TH1*)signal->First();
  while(tmp) {
    for(int i = 0; i < tmp->GetXaxis()->GetNbins(); i++) {
      tmp->SetBinContent(i+1,tmp->GetBinContent(i+1)/tmp->GetBinWidth(i+1));
      tmp->SetBinError(i+1,tmp->GetBinError(i+1)/tmp->GetBinWidth(i+1));
    }
    tmp = (TH1*)signal->After(tmp);
  }
   
}




int Plotter::getSize() {
  return FileList[0]->GetSize() + FileList[1]->GetSize() + FileList[2]->GetSize();
}


vector<string> Plotter::getFilenames(string option) {
  vector<string> filenames;
  TFile* tmp = NULL;
  TList* worklist = NULL;
  if(option == "data") worklist = FileList[0];
  else if(option == "background") worklist = FileList[1];
  else if(option == "signal") worklist = FileList[2];
  else if(option == "all") {
    for(int i = 0; i < 3; i++) {
      tmp = (TFile*)FileList[i]->First();
      while(tmp) {
	filenames.push_back(tmp->GetTitle());
	tmp = (TFile*)FileList[i]->After(tmp);
      }
    }
    return filenames;
    
  } else {
    cout << "Error in filename option, returning empty vector" << endl;
    return filenames;
  }
  tmp = (TFile*)worklist->First();
  while(tmp) {
    filenames.push_back(tmp->GetTitle());
    tmp = (TFile*)worklist->After(tmp);
  }
  worklist = NULL;
  return filenames;
  
}

void Plotter::setStyle(Style& style) {
  this->styler = style;
}

void Plotter::addFile(Normer& norm) {
  string filename = norm.output;
  if(norm.use == 0) {
    cout << filename << ": Not all files found" << endl;
    return;
  } 

  while(filename.find("#") != string::npos) {
    filename.erase(filename.find("#"), 1);
  }

  TFile* normedFile = NULL;
  if(norm.use == 1) {
    norm.print();
    norm.FileList = new TList();
    for(vector<string>::iterator name = norm.input.begin(); name != norm.input.end(); ++name) {
      norm.FileList->Add(TFile::Open(name->c_str()));
    }
    
    normedFile = new TFile(filename.c_str(), "RECREATE");
    norm.MergeRootfile(normedFile);
  } else if(norm.use == 2) {
    cout << filename << " is already Normalized" << endl << endl;;
    normedFile = new TFile(filename.c_str());
  }

  normedFile->SetTitle(norm.output.c_str());
  
  if(norm.type == "data") FileList[0]->Add(normedFile);
  else if(norm.type == "bg") FileList[1]->Add(normedFile);
  else if(norm.type == "sig") FileList[2]->Add(normedFile);
  

}


string Plotter::newLabel(string stringkey) {
  string particle = "";

  smatch m;
  regex part ("(Di)?(Tau(Jet)?|Muon|Electron|Jet)");

  regex e ("^(.+?)(1|2)?Energy$");
  regex n ("^N(.+)$");
  regex charge ("^(.+?)(1|2)?Charge");
  regex mass ("(.+?)(Not)?Mass");
  regex zeta ("(.+?)(P)?Zeta(1D|Vis)?");
  regex deltar ("(.+?)DeltaR");
  regex MetMt ("^(([^_]*?)(1|2)|[^_]+_(.+?)(1|2))MetMt$");
  regex eta ("^(.+?)(Delta)?(Eta)");
  regex phi ("^(([^_]*?)|[^_]+_(.+?))(Delta)?(Phi)");
  regex cosdphi ("(.+?)(CosDphi)(.*)");
  regex pt ("^(.+?)(Delta)?(Pt)(Div)?.*$");
  regex osls ("^(.+?)OSLS");
  regex zdecay ("^[^_]_(.+)IsZdecay$");


  if(regex_match(stringkey, m, e)) {
    return "E(" + turnLatex(m[1].str()) + ") [GeV]";
  }
  else if(regex_match(stringkey, m, n)) {
    return "N(" + turnLatex(m[1].str()) + ")";
  }
  else if(regex_match(stringkey, m, charge)) {
    return  "charge(" + turnLatex(m[1].str())+ ") [e]";
  }
  else if(regex_match(stringkey, m, mass)) {
    return ((m[2].str() == "Not") ? "Not Reconstructed M(": "M(") + listParticles(m[1].str()) + ") [GeV]";
  }
  else if(regex_match(stringkey, m, zeta)) {
    return m[2].str() + "#zeta" +((m[3].str() != "") ? "_{" + m[3].str() + "}":"") + "(" + listParticles(m[1].str()) + ")";
  }
  else if(regex_match(stringkey, m, deltar)) {
    return "#DeltaR("+ listParticles(m[1].str()) + ")";
  }
  else if(regex_match(stringkey, m, MetMt)) {
    return "M_t(" + turnLatex(m[2].str()+m[4].str()) + ") [GeV]";
  }
  else if(regex_match(stringkey, m, eta))  {
    particle += (m[2].str() != "") ? "#Delta" : "";
    particle += "#eta(" + listParticles(m[1].str()) + ")";
    return particle;
  }
  else if(regex_match(stringkey, m, phi))  {
    if(m[4].str() != "") particle = "#Delta";
    particle += "#phi(" + listParticles(m[2].str()+m[3].str()) + ")";
    return particle;
  }
  else if(regex_match(stringkey, m, cosdphi)) {
    return "cos(#Delta#phi(" + listParticles(m[1].str()) + "))";
  }
  else if(regex_match(stringkey, m, pt)) {
    if(m[4].str() != "") particle += "#frac{#Delta p_{T}}{#Sigma p_{T}}(";
    else if(m[2] != "") particle += "#Delta p_{T}(";
    else particle += "p_{T}(";
    particle += listParticles(m[1].str()) + ") [GeV]";
    return particle;
  } 
  else if(stringkey.find("Met") != string::npos) return "#slash{E}_{T} [GeV]";
  else if(stringkey.find("MHT") != string::npos) return "#slash{H}_{T} [GeV]";
  else if(stringkey.find("HT") != string::npos) return "H_{T} [GeV]";
  else if(stringkey.find("Meff") != string::npos) return "M_{eff} [GeV]";
  else if(regex_match(stringkey, m, osls)) {
    particle = m[1].str();
    string full = "";
    while(regex_search(particle, m, part)) {
      full += "q_{" + turnLatex(m[0].str()) + "} ";
      particle = m.suffix().str();
    }
    return full;
  } else if(regex_match(stringkey, m, zdecay)) {
    return listParticles(m[1].str()) + "is Z Decay";
  }
  cout << stringkey << "|" << endl;
  return stringkey;

}


string Plotter::listParticles(string toParse) {
  smatch m;
  regex part ("(Di)?(Tau(Jet)?|Muon|Electron|Jet|Met)");
  bool first = true;
  string final = "";

  while(regex_search(toParse,m,part)) {
    if(first) first = false;
    else final += ", ";
    final += turnLatex(m[0].str());
    toParse = m.suffix().str();
  }
  return final;
}


