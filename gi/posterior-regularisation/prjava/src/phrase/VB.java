package phrase;

import gnu.trove.TIntArrayList;

import io.FileUtil;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;

import org.apache.commons.math.special.Gamma;

import phrase.Corpus.Edge;

public class VB {

	public static int MAX_ITER=400;
	
	/**@brief
	 * hyper param for beta
	 * where beta is multinomial
	 * for generating words from a topic
	 */
	public double lambda=0.1;
	/**@brief
	 * hyper param for theta
	 * where theta is dirichlet for z
	 */
	public double alpha=0.0001;
	/**@brief
	 * variational param for beta
	 */
	private double rho[][][];
	private double digamma_rho[][][];
	private double rho_sum[][];
	/**@brief
	 * variational param for z
	 */
	//private double phi[][];
	/**@brief
	 * variational param for theta
	 */
	private double gamma[];
	private static double VAL_DIFF_RATIO=0.005;
	
	private int n_positions;
	private int n_words;
	private int K;
	private ExecutorService pool;
	
	private Corpus c;
	public static void main(String[] args) {
	//	String in="../pdata/canned.con";
		String in="../pdata/btec.con";
		String out="../pdata/vb.out";
		int numCluster=25;
		Corpus corpus = null;
		File infile = new File(in);
		try {
			System.out.println("Reading concordance from " + infile);
			corpus = Corpus.readFromFile(FileUtil.reader(infile));
			corpus.printStats(System.out);
		} catch (IOException e) {
			System.err.println("Failed to open input file: " + infile);
			e.printStackTrace();
			System.exit(1);
		}
		
		VB vb=new VB(numCluster, corpus);
		int iter=20;
		for(int i=0;i<iter;i++){
			double obj=vb.EM();
			System.out.println("Iter "+i+": "+obj);
		}
		
		File outfile = new File (out);
		try {
			PrintStream ps = FileUtil.printstream(outfile);
			vb.displayPosterior(ps);
		//	ps.println();
		//	c2f.displayModelParam(ps);
			ps.close();
		} catch (IOException e) {
			System.err.println("Failed to open output file: " + outfile);
			e.printStackTrace();
			System.exit(1);
		}
	}

	public VB(int numCluster, Corpus corpus){
		c=corpus;
		K=numCluster;
		n_positions=c.getNumContextPositions();
		n_words=c.getNumWords();
		rho=new double[K][n_positions][n_words];
		//to init rho
		//loop through data and count up words
		double[] phi_tmp=new double[K];
		for(int i=0;i<K;i++){
			for(int pos=0;pos<n_positions;pos++){
				Arrays.fill(rho[i][pos], lambda);
			}
		}
		for(int d=0;d<c.getNumPhrases();d++){
			List<Edge>doc=c.getEdgesForPhrase(d);
			for(int n=0;n<doc.size();n++){
				TIntArrayList context=doc.get(n).getContext();
				arr.F.randomise(phi_tmp);
				for(int i=0;i<K;i++){
					for(int pos=0;pos<n_positions;pos++){
						rho[i][pos][context.get(pos)]+=phi_tmp[i];
					}
				}
			}
		}
		
	}
	
	private double inference(int phraseID, double[][] phi, double[] gamma)
	{
		List<Edge > doc=c.getEdgesForPhrase(phraseID);
		for(int i=0;i<phi.length;i++){
			for(int j=0;j<phi[i].length;j++){
				phi[i][j]=1.0/K;
			}
		}
		Arrays.fill(gamma,alpha+1.0/K);
		
		double digamma_gamma[]=new double[K];
		
		double gamma_sum=digamma(arr.F.l1norm(gamma));
		for(int i=0;i<K;i++){
			digamma_gamma[i]=digamma(gamma[i]);
		}
		double gammaSum[]=new double [K];
		double prev_val=0;
		double obj=0;
		
		for(int iter=0;iter<MAX_ITER;iter++){
			prev_val=obj;
			obj=0;
			Arrays.fill(gammaSum,0.0);
			for(int n=0;n<doc.size();n++){
				TIntArrayList context=doc.get(n).getContext();
				double phisum=0;
				for(int i=0;i<K;i++){
					double sum=0;
					for(int pos=0;pos<n_positions;pos++){
						int word=context.get(pos);
						sum+=digamma_rho[i][pos][word]-rho_sum[i][pos];
					}
					sum+= digamma_gamma[i]-gamma_sum;
					phi[n][i]=sum;
					
					if (i > 0){
	                    phisum = log_sum(phisum, phi[n][i]);
					}
	                else{
	                    phisum = phi[n][i];
	                }
					
				}//end of  a word
				
				for(int i=0;i<K;i++){
					phi[n][i]=Math.exp(phi[n][i]-phisum);
					gammaSum[i]+=phi[n][i];
				}
				
			}//end of doc
			
			for(int i=0;i<K;i++){
				gamma[i]=alpha+gammaSum[i];
			}
			gamma_sum=digamma(arr.F.l1norm(gamma));
			for(int i=0;i<K;i++){
				digamma_gamma[i]=digamma(gamma[i]);
			}
			//compute objective for reporting

			obj=0;
			
			for(int i=0;i<K;i++){
				obj+=(alpha-1)*(digamma_gamma[i]-gamma_sum);
			}
			
			
			for(int n=0;n<doc.size();n++){
				TIntArrayList context=doc.get(n).getContext();
				
				for(int i=0;i<K;i++){
					//entropy of phi + expected log likelihood of z
					obj+=phi[n][i]*(digamma_gamma[i]-gamma_sum);
					
					if(phi[n][i]>1e-10){
						obj+=phi[n][i]*Math.log(phi[n][i]);
					}
					
					double beta_sum=0;
					for(int pos=0;pos<n_positions;pos++){
						int word=context.get(pos);
						beta_sum+=(digamma(rho[i][pos][word])-rho_sum[i][pos]);
					}
					obj+=phi[n][i]*beta_sum;
				}
			}
			
			obj-=log_gamma(arr.F.l1norm(gamma));
			for(int i=0;i<K;i++){
				obj+=Gamma.logGamma(gamma[i]);
				obj-=(gamma[i]-1)*(digamma_gamma[i]-gamma_sum);
			}
			
//			System.out.println(phraseID+": "+obj);
			if(iter>0 && (obj-prev_val)/Math.abs(obj)<VAL_DIFF_RATIO){
				break;
			}
		}//end of inference loop
		
		return obj;
	}//end of inference
	
	/**
	 * @return objective of this iteration
	 */
	public double EM(){
		double emObj=0;
		if(digamma_rho==null){
			digamma_rho=new double[K][n_positions][n_words];
		}
		for(int i=0;i<K;i++){
			for (int pos=0;pos<n_positions;pos++){
				for(int j=0;j<n_words;j++){
					digamma_rho[i][pos][j]= digamma(rho[i][pos][j]);
				}
			}
		}
		
		if(rho_sum==null){
			rho_sum=new double [K][n_positions];
		}
		for(int i=0;i<K;i++){
			for(int pos=0;pos<n_positions;pos++){
				rho_sum[i][pos]=digamma(arr.F.l1norm(rho[i][pos]));
			}
		}

		//E
		double exp_rho[][][]=new double[K][n_positions][n_words];
		if (pool == null)
		{
			for (int d=0;d<c.getNumPhrases();d++)
			{		
				List<Edge > doc=c.getEdgesForPhrase(d);
				double[][] phi = new double[doc.size()][K];
				double[] gamma = new double[K];
				
				emObj += inference(d, phi, gamma);
				
				for(int n=0;n<doc.size();n++){
					TIntArrayList context=doc.get(n).getContext();
					for(int pos=0;pos<n_positions;pos++){
						int word=context.get(pos);
						for(int i=0;i<K;i++){	
							exp_rho[i][pos][word]+=phi[n][i];
						}
					}
				}
				//if(d!=0 && d%100==0)  System.out.print(".");
				//if(d!=0 && d%1000==0) System.out.println(d);
			}
		}
		else // multi-threaded version of above loop
		{
			class PartialEStep implements Callable<PartialEStep>
			{
				double[][] phi;
				double[] gamma;
				double obj;
				int d;
				PartialEStep(int d) { this.d = d; }

				public PartialEStep call()
				{
					phi = new double[c.getEdgesForPhrase(d).size()][K];
					gamma = new double[K];
					obj = inference(d, phi, gamma);
					return this;
				}			
			}

			List<Future<PartialEStep>> jobs = new ArrayList<Future<PartialEStep>>();
			for (int d=0;d<c.getNumPhrases();d++)
				jobs.add(pool.submit(new PartialEStep(d)));
		
			for (Future<PartialEStep> job: jobs)
			{
				try {
					PartialEStep e = job.get();
					
					emObj += e.obj;				
					List<Edge> doc = c.getEdgesForPhrase(e.d);
					for(int n=0;n<doc.size();n++){
						TIntArrayList context=doc.get(n).getContext();
						for(int pos=0;pos<n_positions;pos++){
							int word=context.get(pos);
							for(int i=0;i<K;i++){	
								exp_rho[i][pos][word]+=e.phi[n][i];
							}
						}
					}
				} catch (ExecutionException e) {
					System.err.println("ERROR: E-step thread execution failed.");
					throw new RuntimeException(e);
				} catch (InterruptedException e) {
					System.err.println("ERROR: Failed to join E-step thread.");
					throw new RuntimeException(e);
				}
			}
		}	
	//	System.out.println("EM Objective:"+emObj);
		
		//M
		for(int i=0;i<K;i++){
			for(int pos=0;pos<n_positions;pos++){
				for(int j=0;j<n_words;j++){
					rho[i][pos][j]=lambda+exp_rho[i][pos][j];
				}
			}
		}
		
		//E[\log p(\beta|\lambda)] - E[\log q(\beta)]
		for(int i=0;i<K;i++){
			double rhoSum=0;
			for(int pos=0;pos<n_positions;pos++){
				for(int j=0;j<n_words;j++){
					rhoSum+=rho[i][pos][j];
				}
				double digamma_rhoSum=Gamma.digamma(rhoSum);
				emObj-=Gamma.logGamma(rhoSum);
				for(int j=0;j<n_words;j++){
					emObj+=(lambda-rho[i][pos][j])*(Gamma.digamma(rho[i][pos][j])-digamma_rhoSum);
					emObj+=Gamma.logGamma(rho[i][pos][j]);
				}
			}
		}
		
		return emObj;
	}//end of EM
	
	public void displayPosterior(PrintStream ps)
	{	
		for(int d=0;d<c.getNumPhrases();d++){
			List<Edge > doc=c.getEdgesForPhrase(d);
			double[][] phi = new double[doc.size()][K];
			for(int i=0;i<phi.length;i++)
				for(int j=0;j<phi[i].length;j++)
					phi[i][j]=1.0/K;
			double[] gamma = new double[K];

			inference(d, phi, gamma);

			for(int n=0;n<doc.size();n++){
				Edge edge=doc.get(n);
				int tag=arr.F.argmax(phi[n]);
				ps.print(edge.getPhraseString());
				ps.print("\t");
				ps.print(edge.getContextString(true));

				ps.println(" ||| C=" + tag);
			}
		}
	}

	double log_sum(double log_a, double log_b)
	{
	  double v;

	  if (log_a < log_b)
	      v = log_b+Math.log(1 + Math.exp(log_a-log_b));
	  else
	      v = log_a+Math.log(1 + Math.exp(log_b-log_a));
	  return(v);
	}
		
	double digamma(double x)
	{
	    double p;
	    x=x+6;
	    p=1/(x*x);
	    p=(((0.004166666666667*p-0.003968253986254)*p+
		0.008333333333333)*p-0.083333333333333)*p;
	    p=p+Math.log(x)-0.5/x-1/(x-1)-1/(x-2)-1/(x-3)-1/(x-4)-1/(x-5)-1/(x-6);
	    return p;
	}
	
	double log_gamma(double x)
	{
	     double z=1/(x*x);

	    x=x+6;
	    z=(((-0.000595238095238*z+0.000793650793651)
		*z-0.002777777777778)*z+0.083333333333333)/x;
	    z=(x-0.5)*Math.log(x)-x+0.918938533204673+z-Math.log(x-1)-
	    Math.log(x-2)-Math.log(x-3)-Math.log(x-4)-Math.log(x-5)-Math.log(x-6);
	    return z;
	}

	public void useThreadPool(ExecutorService threadPool) 
	{
		pool = threadPool;
	}
}//End of  class
