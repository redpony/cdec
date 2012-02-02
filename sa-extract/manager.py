import csuf
import cdat

class Sampler(object):
	'''A Sampler implements a logic for choosing
	samples from a population range'''

	def __init__(self):
		pass

	def registerContext(self, context_manager):
		self.context_manager = context_manager

	def sample(self, phrase_location):
		return cintlist.CIntList()



class Extractor(object):
	'''Extractor is responsible for extracting rules
	from a given context; once a sentence id/location
	is found for the source fwords, extractor is 
	responsible for producing any matching rule(s).
	Optionally, extractor may return an empty list'''

	def __init__(self):
		pass

	def registerContext(self, context_manager):
		self.context_manager = context_manager

	def extract(self, fwords, loc):
		return []
	


class RuleFactory(object):
	'''RuleFactory is a class that manages the
	generation of translation rules, using the Context
	and (optionally) any of its contained classes or
	data.  The RuleFactory is responsible for handling
	any caching (i.e. when presented with an input
	sentence, it may lookup a rule from its cache
	rather than extracting a new rule)'''

	def __init__(self):
		self.num_lookups = 0
		self.num_extractions = 0
		self.num_rules = 0
		self.time = 0.0


	def registerContext(self, context_manager):
		self.context_manager = context_manager


	def input(self, fwords):
		'''Manages the process of enumerating
		rules for a given input sentence, and
		looking them with calls to Context,
		Sampler, and Extractor'''
		return []


class ContextManager(object):

	def __init__(self, ffile, efile, extractor=None, sampler=None, rulefactory=None, from_binary=False):
		# NOTE: Extractor does not have a default value because
		# the only nontrivial extractor right now depends on an
		# alignment file

		self.fsarray = csuf.SuffixArray(ffile, from_binary)
		self.edarray = cdat.DataArray(efile, from_binary)

		self.factory = rulefactory
		self.factory.registerContext(self)

		self.sampler = sampler
		self.sampler.registerContext(self)

		self.models = []
		self.owner = None


	def add_model(self, model):
		if self.owner is None:
			self.owner = model
		model_id = len(self.models)
		self.models.append(model)
		return model_id


	def input(self, model, fwords, meta):
		if model != self.owner:
			return
		self.fwords = fwords
		self.factory.input(self.fwords, meta)



