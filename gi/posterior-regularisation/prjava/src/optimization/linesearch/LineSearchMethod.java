package optimization.linesearch;


public interface LineSearchMethod {
	
	double getStepSize(DifferentiableLineSearchObjective o);
	
	public double getInitialGradient();
	public double getPreviousInitialGradient();
	public double getPreviousStepUsed();
	
	public void setInitialStep(double initial);
	public void reset();
}
