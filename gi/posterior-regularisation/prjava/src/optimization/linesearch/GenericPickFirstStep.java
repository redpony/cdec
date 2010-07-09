package optimization.linesearch;


public class GenericPickFirstStep{
	double _initValue;
	public GenericPickFirstStep(double initValue) {
		_initValue = initValue;
	}
	
	public double getFirstStep(LineSearchMethod ls){
		return _initValue;
	}
	public void collectInitValues(LineSearchMethod ls){
		
	}
	
	public void collectFinalValues(LineSearchMethod ls){
		
	}
}
