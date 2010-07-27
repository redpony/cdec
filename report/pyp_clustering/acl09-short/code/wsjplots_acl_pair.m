%wsj_lengths = load([ 'wsj_lengths.dat']);
%save([ 'wsj_lengths.mat'],'wsj_lengths');
load wsj
load wsj_lengths

figure(1)
clf 

subplot(1,2,1);
hold on

for i = 3:6

    b = 10^(i-1)

  disp(['Loading results for b = ' num2str(b) ]);
%%%  uncomment these lines if .mat file is not yet generated. %%%
  %typecountrecord= load([ 'outputs/typecountrecordwsjflat0.0.' num2str(b) '.0.dat']);
  %typecountrecordmean = mean(typecountrecord(:,:));
  %save([ 'outputs/typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat'],'typecountrecordmean');
  load([ 'outputs/typecountrecordmeanwsjflat0.0.' num2str(b) '.0.mat']);
   
  % plot lines for CRP exact prediction using summation
  [logbins predicted dummy] = logbinmean(counts, crppred(counts,b),20,20);
  ph = plot(log10(logbins),log10(predicted),'r');
  set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5)

  % plot lines for CRP Antoniak prediction
  [logbins predicted dummy] = logbinmean(counts, antoniakpred(counts,b),20,20);
  ph = plot(log10(logbins),log10(predicted),'r');
  set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5,'linestyle','--')

  %plot lines for incorrect CRP Antoniak prediction (ACL07)
  %[logbins predicted dummy] = logbinmean(counts, noP0pred(counts,b),20,20);
  %ph = plot(log10(logbins),log10(predicted),'r');
  %set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5,'linestyle',':')

  % plot lines for CRP Cohn prediction
  %[logbins predicted dummy] = logbinmean(counts, cohnpred(counts,b),20,20);
  %ph = plot(log10(logbins),log10(predicted),'r');
  %set(ph,'color',[0.2 0.2 1],'linewidth',1.5,'linestyle','.')

   %plot emprical counts with error bars
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20);
  errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');
end

set(gca,'xtick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'ytick',log10([.1:.1:1 2:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'xlim',[-0.1 3.5])
set(gca,'ylim',[-1.1 1.5])
set(gca,'FontSize',14)
set(gca,'xticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
set(gca,'yticklabel', {'0.1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
    '1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
%title('Chinese restaurant process adaptor')
ylabel('Mean number of lexical entries')
xlabel('Word frequency (n_w)')
legend('Expectation','Antoniak approx.','Empirical','Location','NorthWest')
box on


subplot(1,2,2);
hold on

for i =3:6

  b = 10^(i-1)

  disp(['Loading results for b = ' num2str(b) ]);
%%%  uncomment these lines if .mat file is not yet generated. %%%
  %typecountrecord= load([ 'outputs/typecountrecordwsjpeak0.0.' num2str(b) '.0.dat']);
  %typecountrecordmean = mean(typecountrecord(:,:));
  %save([ 'outputs/typecountrecordmeanwsjpeak0.0.' num2str(b) '.0.mat'],'typecountrecordmean');
  load([ 'outputs/typecountrecordmeanwsjpeak0.0.' num2str(b) '.0.mat']);
   
  % plot lines for CRP exact prediction using summation
  [logbins predicted dummy] = logbinmean(counts, crppred(counts,b),20,20);
  ph = plot(log10(logbins),log10(predicted),'r');
  set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5)

   %plot lines for incorrect CRP Antoniak prediction (ACL07)
  [logbins predicted dummy] = logbinmean(counts, noP0pred(counts,b),20,20);
  ph = plot(log10(logbins),log10(predicted),'r');
  set(ph,'color',[0.7 0.7 0.7],'linewidth',1.5,'linestyle','-.')

  %plot emprical counts with error bars
  [logbins meanval seval] = logbinmean(counts,typecountrecordmean,20,20);
  errorbar(log10(logbins),log10(meanval),log10(meanval+seval)-log10(meanval),log10(meanval-seval)-log10(meanval),'k.');
end

set(gca,'xtick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'ytick',log10([1:10 20:10:100 200:100:1000 2000:1000:5000]))
set(gca,'xlim',[-0.1 3.5])
set(gca,'ylim',[-.1 2.5])
set(gca,'FontSize',14)
set(gca,'xticklabel', {'1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
set(gca,'yticklabel', {...%'0.1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
    '1',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ...
		    '10',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '100', ...
		    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1000', ...
		    ' ', ' ', ' ', ' '});
%title('Chinese restaurant process adaptor')
ylabel('Mean number of lexical entries')
xlabel('Word frequency (n_w)')
legend('Expectation','GGJ07 approx.','Empirical','Location','NorthWest')
box on
%axis square