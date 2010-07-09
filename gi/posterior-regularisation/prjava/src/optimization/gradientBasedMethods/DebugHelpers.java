package optimization.gradientBasedMethods;

import java.util.ArrayList;

import optimization.util.MathUtils;



public class DebugHelpers {
	public static void getLineSearchGraph(Objective o, double[] direction, 
			double[] parameters, double originalObj,
			double originalDot, double c1, double c2){
		ArrayList<Double> stepS = new ArrayList<Double>();
		ArrayList<Double> obj = new ArrayList<Double>();
		ArrayList<Double> norm = new ArrayList<Double>();
		double[] gradient = new double[o.getNumParameters()];
		double[] newParameters = parameters.clone();
		MathUtils.plusEquals(newParameters,direction,0);
		o.setParameters(newParameters);
		double minValue = o.getValue();
		int valuesBiggerThanMax = 0;
		for(double step = 0; step < 2; step +=0.01 ){
			newParameters = parameters.clone();
			MathUtils.plusEquals(newParameters,direction,step);
			o.setParameters(newParameters);
			double newValue = o.getValue();
			gradient = o.getGradient();
			double newgradDirectionDot = MathUtils.dotProduct(gradient,direction);
			stepS.add(step);
			obj.add(newValue);
			norm.add(newgradDirectionDot);
			if(newValue <= minValue){
				minValue = newValue;
			}else{
				valuesBiggerThanMax++;
			}
			
			if(valuesBiggerThanMax > 10){
				break;
			}
			
		}
		System.out.println("step\torigObj\tobj\tsuffdec\tnorm\tcurvature1");
		for(int i = 0; i < stepS.size(); i++){
			double cnorm= norm.get(i); 
			System.out.println(stepS.get(i)+"\t"+originalObj +"\t"+obj.get(i) + "\t" + 
					(originalObj + originalDot*((Double)stepS.get(i))*c1) +"\t"+Math.abs(cnorm) +"\t"+c2*Math.abs(originalDot));
		}
	}
	
	public static double[] getNumericalGradient(Objective o, double[] parameters, double epsilon){
		int nrParameters = o.getNumParameters();
		double[] gradient = new double[nrParameters];
		double[] newParameters;
		double originalValue = o.getValue();
		for(int parameter = 0; parameter < nrParameters; parameter++){
			newParameters = parameters.clone();
			newParameters[parameter]+=epsilon;
			o.setParameters(newParameters);
			double newValue = o.getValue();
			gradient[parameter]=(newValue-originalValue)/epsilon;
		}	
		return gradient;
	}
}
