#include <fstream> 
#include <string>
#include <iostream>
#include <cmath>

class datafile
{  
  public:
    // constructor
    datafile(string,float);

    // UI functions 
    void plot_data(char);
    void plot_elab(); 
    
    // mean
    void fill_mean(int);
    void clean_quad();


    
    // graphs
    TGraph * gr_raw;  // actual data
    TGraph * gr_der;  // "derivative"of data
    TH1F   * h_der;   // histogram of "derivatives"
    TGraph * gr_elab; // data after elaboration

    // ntuple 
    TNtuple* nt_data = new TNtuple("nt_data",
                    "data from Arduino",
                    "Time:Weight:Temperature:Humidity");

  //protected:

    // get parameters
    int get_n_lines() { return lines; } 
    int get_n_elab() { return n_elab; }
    float get_x(int i) { return x[i]; }
    float get_y(int i) { return y[i]; }
    float get_xel(int i) { return x_elab[i]; }
    float get_yel(int i) { return y_elab[i]; }
    float get_u() { return unit; }

        // derivative
    double get_derivative();




  //private:
    float unit; // to rescale i.e. the time
    /* EXAMPLE: acquisition every minute 
     * unit=1./60 gives the time on the axis in units of 1 hour
     */

    // actual data
    float * x;
    float * y;
    float * T;
    float * hum;
    float * dy; // derivative

    // data after elaboration
    int n_elab;
    float * x_elab;
    float * y_elab;

    const char * filename;// name of the file to read from
    int headlines, lines; // to be set by count_lines()
    void count_lines();   // counts actual lines vs. header lines ("# ...") 
                           
    void fill_data(float);
    void fill_ntuple(float);

    // iterators: start, border, explorer
    int is, ib, ie;
    void explore(TF1*, float, int);

    //float a, b, c;
    TF1 * quad = new TF1("quad","[2]*x*x+[1]*x+[0]",0,10);
    //  void quadratic_fit(float*, float*, int, int, float&, float&, float&);
};


// CONSTRUCTOR
// sets: filename, unit
// fills x, y 
datafile::datafile(string filestring, float myunit):
  filename(filestring.c_str()),unit(myunit)
{
  count_lines();
  x   = new float[lines];
  y   = new float[lines];
  T   = new float[lines];
  hum = new float[lines];
  fill_data(unit); 
  fill_ntuple(unit);
}

// performs a count of headlines and effective lines
// sets: headlines, lines
void datafile::count_lines()
{
  using namespace std;
  headlines = 0;
  lines = -1;
  string line;
  ifstream myfile(filename);

  while (getline(myfile, line)){
    if(line[0]!='#')
      ++lines;
    else if(line[0]=='#')
      ++headlines;
  }
}

// finds the first point exceeding an arbitrary deviation from a given function
// f: the desired f(x) fitting the data
// sigma: the maximum deviation allowed
// howfar: number of times sigma can be exceeded 
//         before considering it a consistent behaviour//
void datafile::explore(TF1 * f, float sigma, int howfar=5)
{
  int count = 0;
  float error = 0;
  while( count<howfar ){
    ++ie;
    //cerr << "f = " <<  (*f)(x[ie]) << " y[ie] " << y[ie] << endl;
    error = abs( (*f)(x[ie]) - y[ie] );
    //cerr << "cumulative " << cumulative << " sigma " << sigma << endl;
    if( error  > sigma ) ++count;
  }
} 

// extracts data from the file and creates a corresponding TGraph
// sets: gr_raw
// REQUIRES: headlines, lines
void datafile::fill_data(float unit)
{
  using namespace std;
  ifstream myfile(filename);
  string temp;

  for (int i=0; i< headlines; i++){
    getline(myfile, temp);
  }
  for (int i =0; i<lines; i++){
    x[i] = i*unit; // rescale x
    myfile >> y[i] >> T[i] >> hum[i];
  }
  gr_raw = new TGraph (lines, x, y);	
}

void datafile::fill_ntuple(float unit)
{
  using namespace std;
  ifstream myfile(filename);
  string temp;
  for (int i=0; i< headlines; i++){
    getline(myfile, temp);
  }
  float time, weight, temperature, humidity;
  for (int i =0; i<lines; i++){
    time = i*unit; // rescale time
    myfile >> weight;
    myfile >> temperature;
    myfile >> humidity;
    cerr << "i=" << i << ": " << time << " " 
                              << weight << " "
                              << temperature << " "
                              << humidity << endl;
    nt_data->Fill(time,weight,temperature,humidity);
  }
}



// plots gr_raw (actual data)
// mode: '0'  normal plot
//       's'  option "same" passed to TGraph->Draw
void datafile::plot_data(char mode = '0')
{
  switch(mode){
    case '0': 
      gr_raw -> Draw();
      break;
    case 's':
      gr_raw -> Draw("same");
      break;
  }
}


// plots gr_elab (data after elaboration)
void datafile::plot_elab()
{
  gr_elab -> Draw();
}


// performs a simple mean on the data and fills a TGraph
// REQUIRES: fill_data()
// N: every N points, a mean-point is produced
void datafile::fill_mean(int N)
{
  using namespace std;
  int npoints = float(lines/N);
  n_elab = npoints;
  x_elab = new float[npoints];
  y_elab = new float[npoints];
  float tsum = 0;
  int ireal = 0; //counts from 0 to lines
  for(int index=0; index<npoints; index++){ //counts the points in _elab
    for(int i=0; i<N; i++){ //counts the points over which mean is taken
      tsum += y[ireal];
      ++ireal;
    }
    x_elab[index] = N*(index+1./2);
    y_elab[index] = tsum/int(N);
    tsum = 0;
  }
  gr_elab = new TGraph (n_elab, x_elab, y_elab);	
}



//assumes the first point as the real one
//removes systematic noise fitting with quadratic curves
void datafile::clean_quad()
{
  //assumes the first point as the real one
  //removes systematic noise fitting with quadratic curves
  is =0; 
  ie = 100;
  TFitResultPtr r = gr_raw->Fit(quad,"S","",is*unit,ie*unit);
  while(ie < lines){
    /*float a = quad->GetParameter(2); // x*x
      float b = quad->GetParameter(1); // x
      float c = quad->GetParameter(0); // 1
      */
    explore(quad,0.02,1);   // function, sigma, howfar
    //once parameters are known, removes systematic
    //int n_in_interval = ie - is;
    for(int i=is; i<ie; i++){
      y[i] -= quad->GetParameter(2)*x[i]*x[i] + quad->GetParameter(1)*x[i]; //(*quad)(x[i]);
    }
    is = ie; 
    if( ie+50 < lines ) ie += 50; 
    else ie = lines;
    cout << ie << " corresponding to t = " << ie*100./3600 << endl;
    r = gr_raw->Fit(quad,"S","",is*unit,ie*unit);
  }
  gr_raw = new TGraph(lines,x,y);
}


// fills gr_der with derivative
double datafile::get_derivative()
{

  double d_mean = 0; 
  dy = new float[lines];
  dy[0] = 0;
  h_der = new TH1F("h_der","derivative distribution",1000,-0.01,0.01);
  for(int i=1; i<lines; ++i){
    dy[i] = (y[i] - y[i-1]);//unit;
    d_mean += dy[i];
    //cerr << dy[i] << endl;
    h_der->Fill(dy[i]);
  }
  gr_der = new TGraph(lines,x,dy);
  gr_der->Draw();
  return d_mean/lines;
}







/////////////////////////////////////////////////////
//UI FOR datafile


void plot_file(string NAME, float UNIT=1)
{
  datafile mydata(NAME,UNIT);
  mydata.plot_data();
  mydata.clean_quad();
  mydata.plot_data('s');
}

void plot_mean(string NAME, float UNIT=1, int NMEAN=5)
{
  datafile mydata(NAME,UNIT);
  mydata.fill_mean(NMEAN);
  mydata.plot_elab();
}


//////////////////////














//__________________________//

//OLD plot.cxx
/*
   int * n_lines(const char * name) { 
   int number_of_header = 0;
   int number_of_lines = -1;
   std::string line;
   std::ifstream myfile(name);

   while (std::getline(myfile, line))
   if(line[0]!='#')
   ++number_of_lines;
   else if(line[0]=='#')
   ++number_of_header;
   int * numbers = new int[2];
   numbers[0] = number_of_header;
   numbers[1] = number_of_lines;
   return numbers;
   }

   float** plot(string filestring, float unit=1){

   const char * filename = filestring.c_str();
   int header = n_lines(filename)[0];
   int lines  = n_lines(filename)[1];
   std::cerr << header << "  " << lines << endl;

   std::fstream file (filename, std::fstream::in);

   std::string temp;
   float * x = new float [lines];
   float * y = new float [lines];

   for (int i=0; i< header; i++){
   std::getline(file, temp);
//std::cerr << temp << endl;
}
for (int i =0; i<lines; i++){
x[i] = i*unit; // rescale x
file >> y[i];
std::cerr << x[i] << "   " << y[i] << endl;
}

TGraph * plot = new TGraph (lines, x, y);	
plot -> Draw();

float** data_vect = new float*[2];
data_vect[0] = x;
data_vect[1] = y;

return data_vect;

}

*/
