package hmm;

import gnu.trove.TIntArrayList;
import optimization.gradientBasedMethods.ProjectedGradientDescent;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.ArmijoLineSearchMinimizationAlongProjectionArc;
import optimization.linesearch.InterpolationPickFirstStep;
import optimization.linesearch.LineSearchMethod;
import optimization.projections.SimplexProjection;
import optimization.stopCriteria.CompositeStopingCriteria;
import optimization.stopCriteria.ProjectedGradientL2Norm;
import optimization.stopCriteria.StopingCriteria;
import optimization.stopCriteria.ValueDifference;

public class HMMObjective extends ProjectedObjective{

	
	private static final double GRAD_DIFF = 3;
	public static double INIT_STEP_SIZE=10;
	public static double VAL_DIFF=2000;
	
	private HMM hmm;
	double[] newPoint  ;
	
	//posterior[sent num][tok num][tag]=index into lambda
	private int posteriorMap[][][];
	//projection[word][tag].get(occurence)=index into lambda
	private TIntArrayList projectionMap[][];

	//Size of the simplex
	public double scale=10;
	private SimplexProjection projection;
	
	private int wordFreq[];
	private static int MIN_FREQ=3;
	private int numWordsToProject=0;
	
	private int n_param;
	
	public  double loglikelihood;
	
	public HMMObjective(HMM h){
		hmm=h;
		
		countWords();
		buildMap();

		gradient=new double [n_param];
		projection = new SimplexProjection(scale);
		newPoint  = new double[n_param];
		setInitialParameters(new double[n_param]);
		
	}
	
	/**@brief counts word frequency in the corpus
	 * 
	 */
	private void countWords(){
		wordFreq=new int [hmm.emit[0].length];
		for(int i=0;i<hmm.data.length;i++){
			for(int j=0;j<hmm.data[i].length;j++){
				wordFreq[hmm.data[i][j]]++;
			}
		}
	}
	
	/**@brief build posterior and projection indices
	 * 
	 */
	private void buildMap(){
		//number of sentences hidden states and words
		int n_states=hmm.trans.length;
		int n_words=hmm.emit[0].length;
		int n_sents=hmm.data.length;
		
		n_param=0;
		posteriorMap=new int[n_sents][][];
		projectionMap=new TIntArrayList[n_words][];
		for(int sentNum=0;sentNum<n_sents;sentNum++){
			int [] data=hmm.data[sentNum];
			posteriorMap[sentNum]=new int[data.length][n_states];
			numWordsToProject=0;
			for(int i=0;i<data.length;i++){
				int word=data[i];
				for(int state=0;state<n_states;state++){
					if(wordFreq[word]>MIN_FREQ){
						if(projectionMap[word]==null){
							projectionMap[word]=new TIntArrayList[n_states];
						}
						
						posteriorMap[sentNum][i][state]=n_param;
						if(projectionMap[word][state]==null){
							projectionMap[word][state]=new TIntArrayList();
							numWordsToProject++;
						}
						projectionMap[word][state].add(n_param);
						n_param++;
					}else{
						
						posteriorMap[sentNum][i][state]=-1;
					}
				}
			}
		}
	}
	
	@Override
	public double[] projectPoint(double[] point) {
		// TODO Auto-generated method stub
		for(int i=0;i<projectionMap.length;i++){
			
			if(projectionMap[i]==null){
				//this word is not constrained
				continue;
			}
			
			for(int j=0;j<projectionMap[i].length;j++){
				TIntArrayList instances=projectionMap[i][j];
				double[] toProject = new double[instances.size()];
				
				for (int k = 0; k < toProject.length; k++) {
					//	System.out.print(instances.get(k) + " ");
						toProject[k] = point[instances.get(k)];
				}
				
				projection.project(toProject);
				for (int k = 0; k < toProject.length; k++) {
					newPoint[instances.get(k)]=toProject[k];
				}
			}
		}
		return newPoint;
	}

	@Override
	public double[] getGradient() {
		// TODO Auto-generated method stub
		gradientCalls++;
		return gradient;
	}

	@Override
	public double getValue() {
		// TODO Auto-generated method stub
		functionCalls++;
		return loglikelihood;
	}
	

	@Override
	public String toString() {
		// TODO Auto-generated method stub
		StringBuffer sb = new StringBuffer();
		for (int i = 0; i < parameters.length; i++) {
			sb.append(parameters[i]+" ");
			if(i%100==0){
				sb.append("\n");
			}
		}
		sb.append("\n");
		/*
		for (int i = 0; i < gradient.length; i++) {
			sb.append(gradient[i]+" ");
			if(i%100==0){
				sb.append("\n");
			}
		}
		sb.append("\n");
		*/
		return sb.toString();
	}

	
	/**
	 * @param seq
	 * @return posterior probability of each transition
	 */
	public double [][][]forwardBackward(int sentNum){
		int [] seq=hmm.data[sentNum];
		int n_states=hmm.trans.length;
		double a[][]=new double [seq.length][n_states];
		double b[][]=new double [seq.length][n_states];
		
		int len=seq.length;
		
		boolean  constrained=
			(projectionMap[seq[0]]!=null);

		//initialize the first step
		for(int i=0;i<n_states;i++){
			a[0][i]=hmm.emit[i][seq[0]]*hmm.pi[i];
			if(constrained){
				a[0][i]*=
					Math.exp(- parameters[ posteriorMap[sentNum][0][i] ] );
			}
			b[len-1][i]=1;
		}
		
		loglikelihood+=Math.log(hmm.l1norm(a[0]));		
		hmm.l1normalize(a[0]);
		hmm.l1normalize(b[len-1]);
		
		//forward
		for(int n=1;n<len;n++){
			
			constrained=
				(projectionMap[seq[n]]!=null);
			
			for(int i=0;i<n_states;i++){
				for(int j=0;j<n_states;j++){
					a[n][i]+=hmm.trans[j][i]*a[n-1][j];
				}
				a[n][i]*=hmm.emit[i][seq[n]];
				
				if(constrained){
					a[n][i]*=
						Math.exp(- parameters[ posteriorMap[sentNum][n][i] ] );
				}
				
			}
			loglikelihood+=Math.log(hmm.l1norm(a[n]));
			hmm.l1normalize(a[n]);
		}
		
		//temp variable for e^{-\lambda}
		double factor=1;
		//backward
		for(int n=len-2;n>=0;n--){
			
			constrained=
				(projectionMap[seq[n+1]]!=null);
			
			for(int i=0;i<n_states;i++){
				for(int j=0;j<n_states;j++){
					
					if(constrained){
						factor=
							Math.exp(- parameters[ posteriorMap[sentNum][n+1][j] ] );
					}else{
						factor=1;
					}
					
					b[n][i]+=hmm.trans[i][j]*b[n+1][j]*hmm.emit[j][seq[n+1]]*factor;
					
				}
			}
			hmm.l1normalize(b[n]);
		}
		
		//expected transition 
		double p[][][]=new double [seq.length][n_states][n_states];
		for(int n=0;n<len-1;n++){
			
			constrained=
				(projectionMap[seq[n+1]]!=null);
			
			for(int i=0;i<n_states;i++){
				for(int j=0;j<n_states;j++){
					
					if(constrained){
						factor=
							Math.exp(- parameters[ posteriorMap[sentNum][n+1][j] ] );
					}else{
						factor=1;
					}
					
					p[n][i][j]=a[n][i]*hmm.trans[i][j]*
						hmm.emit[j][seq[n+1]]*b[n+1][j]*factor;
					
				}
			}

			hmm.l1normalize(p[n]);
		}
		return p;
	}

	public void optimizeWithProjectedGradientDescent(){
		LineSearchMethod ls =
			new ArmijoLineSearchMinimizationAlongProjectionArc
				(new InterpolationPickFirstStep(INIT_STEP_SIZE));
		
		OptimizerStats stats = new OptimizerStats();
		
		
		ProjectedGradientDescent optimizer = new ProjectedGradientDescent(ls);
		StopingCriteria stopGrad = new ProjectedGradientL2Norm(GRAD_DIFF);
		StopingCriteria stopValue = new ValueDifference(VAL_DIFF);
		CompositeStopingCriteria compositeStop = new CompositeStopingCriteria();
		compositeStop.add(stopGrad);
		compositeStop.add(stopValue);
		
		optimizer.setMaxIterations(10);
		updateFunction();
		boolean succed = optimizer.optimize(this,stats,compositeStop);
		System.out.println("Ended optimzation Projected Gradient Descent\n" + stats.prettyPrint(1));
		if(succed){
			System.out.println("Ended optimization in " + optimizer.getCurrentIteration());
		}else{
			System.out.println("Failed to optimize");
		}
	}
	
	@Override
	public void setParameters(double[] params) {
		super.setParameters(params);
		updateFunction();
	}
	
	private void updateFunction(){
		
		updateCalls++;
		loglikelihood=0;
	
		for(int sentNum=0;sentNum<hmm.data.length;sentNum++){
			double [][][]p=forwardBackward(sentNum);
			
			for(int n=0;n<p.length-1;n++){
				for(int i=0;i<p[n].length;i++){
					if(projectionMap[hmm.data[sentNum][n]]!=null){
						double posterior=0;
						for(int j=0;j<p[n][i].length;j++){
							posterior+=p[n][i][j];
						}
						gradient[posteriorMap[sentNum][n][i]]=-posterior;
					}
				}
			}
			
			//the last state
			int n=p.length-2;
			for(int i=0;i<p[n].length;i++){
				if(projectionMap[hmm.data[sentNum][n+1]]!=null){
					
					double posterior=0;
					for(int j=0;j<p[n].length;j++){
						posterior+=p[n][j][i];
					}
					gradient[posteriorMap[sentNum][n+1][i]]=-posterior;
				
				}
			}	
		}
		
	}
	
}
