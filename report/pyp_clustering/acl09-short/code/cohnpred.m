function output = cohnpred(input,b)

uniqin = unique(input);
prediction = zeros(max(input),1);

p0=1/30114;
for i = 1:length(uniqin)
  prediction(uniqin(i)) = b*p0*(psi(b*p0+uniqin(i)) - psi(b*p0));
end

output = prediction(input);

