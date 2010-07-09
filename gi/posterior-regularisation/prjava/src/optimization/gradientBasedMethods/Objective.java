package optimization.gradientBasedMethods;


/**
 * Defines an optimization objective:
 * 
 * 
 * @author javg
 *
 */
public abstract  class Objective {

	protected int functionCalls = 0;
	protected int gradientCalls = 0;
	protected int updateCalls = 0;
	
	protected double[] parameters;
	
	//Contains a cache with the gradient
	public double[] gradient;
	int debugLevel = 0;
	
	public void setDebugLevel(int level){
		debugLevel = level;
	}
	
	public int getNumParameters() {
		return parameters.length;
	}

	public double getParameter(int index) {
		return parameters[index];
	}

	public double[] getParameters() {
		return parameters;
	}

	public abstract double[] getGradient( );
	
	public void setParameter(int index, double value) {
		parameters[index]=value;
	}

	public void setParameters(double[] params) {
		if(parameters == null){
			parameters = new double[params.length];
		}
		updateCalls++;
		System.arraycopy(params, 0, parameters, 0, params.length);
	}

	
	public int getNumberFunctionCalls() {
		return functionCalls;
	}

	public int getNumberGradientCalls() {
		return gradientCalls;
	}
	
	public String finalInfoString() {
		return "FE: " + functionCalls + " GE " + gradientCalls + " Params updates" +
		updateCalls;
	}
	public void printParameters() {
		System.out.println(toString());
	}	
	
	public abstract String toString();	
	public abstract double getValue ();
	
	/**
	 * Sets the initial objective parameters
	 * For unconstrained models this just sets the objective params = argument no copying
	 * For a constrained objective project the parameters and then set
	 * @param params
	 */
	public  void setInitialParameters(double[] params){
		parameters = params;
	}

}
