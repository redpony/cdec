package optimization.linesearch;


public class InterpolationPickFirstStep extends GenericPickFirstStep{
	public InterpolationPickFirstStep(double initValue) {
		super(initValue);
	}
	
	public double getFirstStep(LineSearchMethod ls){
		if(ls.getPreviousStepUsed() != -1 && ls.getPreviousInitialGradient()!=0){
			double newStep = Math.min(300, 1.02*ls.getPreviousInitialGradient()*ls.getPreviousStepUsed()/ls.getInitialGradient());
		//	System.out.println("proposing " + newStep);
			return newStep;
			
		}
		return _initValue;
	}
	public void collectInitValues(WolfRuleLineSearch ls){
		
	}
	
	public void collectFinalValues(WolfRuleLineSearch ls){
		
	}
}
