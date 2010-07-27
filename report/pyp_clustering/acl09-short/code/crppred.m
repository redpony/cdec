function output = crppred(input,b)

uniqin = unique(input);
prediction = zeros(max(input),1);

p0=1/30114;
for i = 1:length(uniqin)
  prediction(uniqin(i)) = b*p0*sum(1./((1:uniqin(i))+b*p0-1));
end

output = prediction(input);

