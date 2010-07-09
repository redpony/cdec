package optimization.linesearch;


public class WolfeConditions {
	
	/**
	 * Sufficient Increase number. Default constant
	 */
	
	
	/**
	 * Value for suficient curvature:
	 * 0.9 - For newton and quase netwon methods
	 * 0.1 - Non linear conhugate gradient
	 */
	
	int debugLevel = 0;
	public void setDebugLevel(int level){
		debugLevel = level;
	}
	
	public  static boolean suficientDecrease(DifferentiableLineSearchObjective o, double c1){	
		double value = o.getOriginalValue()+c1*o.getAlpha()*o.getInitialGradient();
//		System.out.println("Sufficient Decrease original "+value+" new "+  o.getCurrentValue());
		return o.getCurrentValue() <= value;
	}
	
	


	public static boolean sufficientCurvature(DifferentiableLineSearchObjective o, double c1, double c2){
//		if(debugLevel >= 2){
//			double current = Math.abs(o.getCurrentGradient());
//			double orig = -c2*o.getInitialGradient();
//			if(current <= orig){
//				return true;
//			}else{
//				System.out.println("Not satistfying curvature condition curvature " + current + " wants " + orig);
//				return false;
//			}
//		}
		return Math.abs(o.getCurrentGradient()) <= -c2*o.getInitialGradient();
	}
	
}
