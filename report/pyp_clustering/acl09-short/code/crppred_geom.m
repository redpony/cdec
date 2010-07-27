function output = crppred_geom(input,lengths,b)


output = zeros(length(input),1);

p0=(1/52).^lengths;
a=b*p0;
for i = 1:length(input)
  output(i) = a(i)*sum(1./((1:input(i))+a(i)-1));
end


