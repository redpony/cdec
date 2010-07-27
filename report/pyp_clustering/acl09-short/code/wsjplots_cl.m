
load wsj

figure(1)
clf 
subplot(1,2,2)
hold on

for i = 1:9
  a = i/10;
  [logbins predicted dummy] = logbinmean(counts,counts.^a,20,20);
  ph = plot(log10(logbins),log10(predicted),'k');
  set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5)
end

for i = 1:9
  a = i/10;  
  disp(['Loading results for a = ' num2str(a) ]);

  typecountrecord= load([ 'typecountrecordwsjflat' num2str(a) '.1.0.dat']);
  
  typecountrecordmean = mean(typecountrecord(500:1000,:));
  
  save([ 'typecountrecordmeanwsjflat' num2str(a) '.1.0.mat'],'typecountrecordmean');
  
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20)
  errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');
  drawnow
end




[logbins meanval seval] = logbinmean(counts,counts,20,20)
[logbins predicted dummy] = logbinmean(counts,counts,20,20)
ph = plot(log10(logbins),log10(predicted),'r');
hold on
errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');

set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5)

set(gca,'xtick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'ytick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'xlim',[-0.1 3.5])
set(gca,'ylim',[-0.1 3.5])
set(gca,'xticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
set(gca,'yticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});

title('Pitman-Yor process adaptor')
ylabel('Mean number of lexical entries')
xlabel('Word frequency (n_w)')
box on

subplot(1,2,1)

for i = 1:5

  b = 10^(i-1)

  disp(['Loading results for b = ' num2str(b) ]);
  typecountrecord= load([ 'typecountrecordwsjflat0.0.' num2str(b) '.0.dat']);

  typecountrecordmean = mean(typecountrecord(500:1000,:));
  save([ 'typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat'],'typecountrecordmean');
  
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20)
  [logbins predicted dummy] = logbinmean(counts,crppred(counts,b),20,20)
%  errorbar(log10(logbins),meanval,seval,'k.');
  hold on
  ph = plot(log10(logbins),log10(predicted),'r');
  %  ph = plot(log10(logbins),predicted,'r');
  set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5)
  errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');
end

set(gca,'xtick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'ytick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'xlim',[-0.1 3.5])
set(gca,'ylim',[-0.1 1.5])
set(gca,'xticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
set(gca,'yticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
title('Chinese restaurant process adaptor')
ylabel('Mean number of lexical entries')
xlabel('Word frequency (n_w)')
box on


