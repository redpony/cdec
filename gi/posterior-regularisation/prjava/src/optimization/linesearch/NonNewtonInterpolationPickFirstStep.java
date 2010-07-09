package optimization.linesearch;

/**
 * Non newtwon since we don't always try 1...
 * Not sure if that is even usefull for newton
 * @author javg
 *
 */
public class NonNewtonInterpolationPickFirstStep extends GenericPickFirstStep{
	public NonNewtonInterpolationPickFirstStep(double initValue) {
		super(initValue);
	}
	
	public double getFirstStep(LineSearchMethod ls){
//		System.out.println("Previous step used " + ls.getPreviousStepUsed());
//		System.out.println("PreviousGradinebt " + ls.getPreviousInitialGradient());
//		System.out.println("CurrentGradinebt " + ls.getInitialGradient());
		if(ls.getPreviousStepUsed() != -1 && ls.getPreviousInitialGradient()!=0){
			double newStep = 1.01*ls.getPreviousInitialGradient()*ls.getPreviousStepUsed()/ls.getInitialGradient();
			//System.out.println("Suggesting " + newStep);
			return newStep;
			
		}
		return _initValue;
	}
	public void collectInitValues(WolfRuleLineSearch ls){
		
	}
	
	public void collectFinalValues(WolfRuleLineSearch ls){
		
	}
}
