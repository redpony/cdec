package phrase;

import gnu.trove.TIntArrayList;

import io.FileUtil;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.List;

import org.apache.commons.math.special.Gamma;

import phrase.Corpus.Edge;

public class VB {

	public static int MAX_ITER=40;
	
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
	public double alpha=0.000001;
	/**@brief
	 * variational param for beta
	 */
	private double rho[][][];
	/**@brief
	 * variational param for z
	 */
	private double phi[][];
	/**@brief
	 * variational param for theta
	 */
	private double gamma[];
	
	private static double VAL_DIFF_RATIO=0.001;
	
	/**@brief
	 * objective for a single document
	 */
	private double obj;
	
	private int n_positions;
	private int n_words;
	private int K;
	
	private Corpus c;
	public static void main(String[] args) {
		String in="../pdata/canned.con";
		//String in="../pdata/btec.con";
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
	
	private void inference(int phraseID){
		List<Edge > doc=c.getEdgesForPhrase(phraseID);
		phi=new double[doc.size()][K];
		for(int i=0;i<phi.length;i++){
			for(int j=0;j<phi[i].length;j++){
				phi[i][j]=1.0/K;
			}
		}
		gamma = new double[K];
		double digamma_gamma[]=new double[K];
		for(int i=0;i<gamma.length;i++){
			gamma[i] = alpha + 1.0/K;
		}
		
		double rho_sum[][]=new double [K][n_positions];
		for(int i=0;i<K;i++){
			for(int pos=0;pos<n_positions;pos++){
				rho_sum[i][pos]=Gamma.digamma(arr.F.l1norm(rho[i][pos]));
			}
		}
		double gamma_sum=Gamma.digamma(arr.F.l1norm(gamma));
		for(int i=0;i<K;i++){
			digamma_gamma[i]=Gamma.digamma(gamma[i]);
		}
		double gammaSum[]=new double [K];
		
		double prev_val=0;
		obj=0;
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
						sum+=Gamma.digamma(rho[i][pos][word])-rho_sum[i][pos];
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
			gamma_sum=Gamma.digamma(arr.F.l1norm(gamma));
			for(int i=0;i<K;i++){
				digamma_gamma[i]=Gamma.digamma(gamma[i]);
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
						beta_sum+=(Gamma.digamma(rho[i][pos][word])-rho_sum[i][pos]);
					}
					obj+=phi[n][i]*beta_sum;
				}
			}
			
			obj-=Gamma.logGamma(arr.F.l1norm(gamma));
			for(int i=0;i<K;i++){
				obj+=Gamma.logGamma(gamma[i]);
				obj-=(gamma[i]-1)*(digamma_gamma[i]-gamma_sum);
			}
			
//			System.out.println(phraseID+": "+obj);
			if(iter>0 && (obj-prev_val)/Math.abs(obj)<VAL_DIFF_RATIO){
				break;
			}
		}//end of inference loop
	}//end of inference
	
	/**
	 * @return objective of this iteration
	 */
	public double EM(){
		double emObj=0;
		
		//E
		double exp_rho[][][]=new double[K][n_positions][n_words];
		for (int d=0;d<c.getNumPhrases();d++){
			inference(d);
			List<Edge>doc=c.getEdgesForPhrase(d);
			for(int n=0;n<doc.size();n++){
				TIntArrayList context=doc.get(n).getContext();
				for(int pos=0;pos<n_positions;pos++){
					int word=context.get(pos);
					for(int i=0;i<K;i++){	
						exp_rho[i][pos][word]+=phi[n][i];
					}
				}
			}
			
			emObj+=obj;
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
			inference(d);
			List<Edge> doc=c.getEdgesForPhrase(d);
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
	  {
	      v = log_b+Math.log(1 + Math.exp(log_a-log_b));
	  }
	  else
	  {
	      v = log_a+Math.log(1 + Math.exp(log_b-log_a));
	  }
	  return(v);
	}
	
}//End of  class
