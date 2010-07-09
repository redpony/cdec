package optimization.gradientBasedMethods;

import optimization.linesearch.LineSearchMethod;



public class GradientDescent extends AbstractGradientBaseMethod{
	
	public GradientDescent(LineSearchMethod lineSearch) {
		this.lineSearch = lineSearch;
	}
		
	public double[] getDirection(){
		for(int i = 0; i< gradient.length; i++){
			direction[i] = -gradient[i];
		}
		return direction;
	}
}
