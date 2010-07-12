import re, sys

class Symbol:
    def __init__(self, nonterm, term=None, var=None):
        assert not (term != None and var != None)
        self.tag = nonterm
        self.token = term
        self.variable = var

    def is_variable(self):
        return self.variable != None

    def __eq__(self, other):
        return self.tag == other.tag and self.token == other.token and self.variable == other.variable

    def __ne__(self, other):
        return not (self == other)

    def __hash__(self):
        return hash((self.tag, self.token, self.variable))

    def __repr__(self):
        return str(self)

    def __cmp__(self, other):
        return cmp((self.tag, self.token, self.variable),
                   (other.tag, other.token, other.variable))

    def __str__(self):
        parts = []
	if False: # DEPENDENCY
	    if self.token:
		parts.append(str(self.token))
	    elif self.variable != None:
		parts.append('#%d' % self.variable)
	    if self.tag:
		parts.append(str(self.tag))
	    return '/'.join(parts)
	else:
	    if self.tag:
		parts.append(str(self.tag))
	    if self.token:
		parts.append(str(self.token))
	    elif self.variable != None:
		parts.append('#%d' % self.variable)
	    return ' '.join(parts)

class TreeNode:
    def __init__(self, data, children=None, order=-1):
        self.data = data
        self.children = []
        self.order = order
        self.parent = None
        if children: self.children = children

    def insert(self, child):
        self.children.append(child)
        child.parent = self

    def leaves(self):
        ls = []
        for node in self.xtraversal():
            if not node.children:
                ls.append(node.data)
        return ls

    def leaf_nodes(self):
        ls = []
        for node in self.xtraversal():
            if not node.children:
                ls.append(node)
        return ls

    def max_depth(self):
        d = 1
        for child in self.children:
            d = max(d, 1 + child.max_depth())
        if not self.children and self.data.token:
            d = 2
        return d

    def max_width(self):
        w = 0
        for child in self.children:
           w += child.max_width()
        return max(1, w)

    def num_internal_nodes(self):
        if self.children:
            n = 1
            for child in self.children:
                n += child.num_internal_nodes()
            return n
        elif self.data.token:
            return 1
        else:
            return 0

    def postorder_traversal(self, visit):
        """
	Postorder traversal; no guarantee that terminals will be read in the
	correct order for dep. trees.
        """
        for child in self.children:
            child.traversal(visit)
	visit(self)

    def traversal(self, visit):
        """
        Preorder for phrase structure trees, and inorder for dependency trees.
        In both cases the terminals will be read off in the correct order.
        """
        visited_self = False
        if self.order <= 0:
            visited_self = True
            visit(self)

        for i, child in enumerate(self.children):
            child.traversal(visit)
            if i + 1 == self.order:
                visited_self = True
                visit(self)

        assert visited_self

    def xpostorder_traversal(self):
        for child in self.children:
            for node in child.xpostorder_traversal():
                yield node
        yield self

    def xtraversal(self):
        visited_self = False
        if self.order <= 0:
            visited_self = True
            yield self

        for i, child in enumerate(self.children):
            for d in child.xtraversal():
                yield d

            if i + 1 == self.order:
                visited_self = True
                yield self

        assert visited_self

    def xpostorder_traversal(self):
        for i, child in enumerate(self.children):
            for d in child.xpostorder_traversal():
                yield d
        yield self

    def edges(self):
        es = []
        self.traverse_edges(lambda h,c: es.append((h,c)))
        return es

    def traverse_edges(self, visit):
        for child in self.children:
            visit(self.data, child.data)
            child.traverse_edges(visit)

    def subtrees(self, include_self=False):
        st = []
        if include_self:
            stack = [self]
        else:
            stack = self.children[:]

        while stack:
            node = stack.pop()
            st.append(node)
            stack.extend(node.children)
        return st

    def find_parent(self, node):
        try:
            index = self.children.index(node)
            return self, index
        except ValueError:
            for child in self.children:
                if isinstance(child, TreeNode):
                    r = child.find_parent(node)
                    if r: return r
        return None

    def is_ancestor_of(self, node):
        if self == node:
            return True
        for child in self.children:
            if child.is_ancestor_of(child):
                return True
        return False

    def find(self, node):
        if self == node:
            return self
        for child in self.children:
            if isinstance(child, TreeNode):
                r = child.find(node)
                if r: return r
            else:
                if child == node:
                   return r
        return None

    def equals_ignorecase(self, other):
        if not isinstance(other, TreeNode):
            return False
        if self.data != other.data:
            return False
        if len(self.children) != len(other.children):
            return False
        for mc, oc in zip(self.children, other.children):
            if isinstance(mc, TreeNode):
                if not mc.equals_ignorecase(oc):
                    return False
            else:
                if mc.lower() != oc.lower():
                    return False
        return True

    def node_number(self, numbering, next=0):
        if self.order <= 0:
            numbering[id(self)] = next
            next += 1

        for i, child in enumerate(self.children):
            next = child.node_number(numbering, next)
            if i + 1 == self.order:
                numbering[id(self)] = next
                next += 1

        return next

    def display_conll(self, out):
        numbering = {}
        self.node_number(numbering)
        next = 0
        self.children[0].traversal(lambda x: \
            out.write('%d\t%s\t%s\t%s\t%s\t_\t%d\tLAB\n' \
             % (numbering[id(x)], x.data.token, x.data.token, 
                x.data.tag, x.data.tag, numbering[id(x.parent)])))
        out.write('\n')

    def size(self):
        sz = 1 
        for child in self.children:
            sz += child.size()
        return sz

    def __eq__(self, other):
        if isinstance(other, TreeNode) and self.data == other.data \
                and self.children == other.children:
            return True
        return False

    def __cmp__(self, other):
        if not isinstance(other, TreeNode): return 1
        n = cmp(self.data, other.data)
        if n != 0: return n
        n = len(self.children) - len(other.children)
        if n != 0: return n
        for sc, oc in zip(self.children, other.children):
            n = cmp(sc, oc)
            if n != 0: return n
        return 0

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash((self.data, tuple(self.children)))

    def __repr__(self):
        return str(self)

    def __str__(self):
        s = '('
        space = False
        if self.order <= 0:
            s += str(self.data)
            space = True
        for i, child in enumerate(self.children):
            if space: s += ' '
            s += str(child)
            space = True
            if i+1 == self.order:
                s += ' ' + str(self.data)
        return s + ')'

def read_PSTs(fname):
    infile = open(fname)
    trees = []
    for line in infile:
        trees.append(parse_PST(line.strip()))
    infile.close()
    return trees

def parse_PST_multiline(infile, hash_is_var=True):
    buf = ''
    num_open = 0
    while True:
        line = infile.readline()
        if not line:
            return None
        buf += ' ' + line.rstrip()
        num_open += line.count('(') - line.count(')')
        if num_open == 0:
            break

    return parse_PST(buf, hash_is_var)

def parse_PST(line, hash_is_var=True):
    line = line.rstrip()
    if not line or line.lower() == 'null':
        return None

    # allow either (a/DT) or (DT a)
    #parts_re = re.compile(r'(\(*)([^/)]*)(?:/([^)]*))?(\)*)$')

    # only allow (DT a)
    parts_re = re.compile(r'(\(*)([^)]*)(\)*)$')

    root = TreeNode(Symbol('TOP'))
    stack = [root]
    for part in line.rstrip().split():
        m = parts_re.match(part)
        #opening, tok_or_tag, tag, closing = m.groups()
        opening, tok_or_tag, closing = m.groups()
	tag = None
        #print 'token', part, 'bits', m.groups()
        for i in opening:
            node = TreeNode(Symbol(None))
            stack[-1].insert(node)
            stack.append(node)

        if tag:
            stack[-1].data.tag = tag
            if hash_is_var and tok_or_tag.startswith('#'):
                stack[-1].data.variable = int(tok_or_tag[1:])
            else:
                stack[-1].data.token = tok_or_tag
        else:
            if stack[-1].data.tag == None:
                stack[-1].data.tag = tok_or_tag
            else:
                if hash_is_var and tok_or_tag.startswith('#'):
                    try:
                        stack[-1].data.variable = int(tok_or_tag[1:])
                    except ValueError: # it's really a token!
                        #print >>sys.stderr, 'Warning: # used for token:', tok_or_tag
                        stack[-1].data.token = tok_or_tag
                else:
                    stack[-1].data.token = tok_or_tag
        
        for i in closing:
            stack.pop()

    #assert str(root.children[0]) == line
    return root.children[0]

def read_DTs(fname):
    infile = open(fname)
    trees = []
    while True:
        t = parse_DT(infile)
        if t: trees.append(t)
        else: break
    infile.close()
    return trees

def read_bracketed_DTs(fname):
    infile = open(fname)
    trees = []
    for line in infile:
        trees.append(parse_bracketed_DT(line))
    infile.close()
    return trees

def parse_DT(infile):
    tokens = [Symbol('ROOT')]
    children = {}

    for line in infile:
        parts = line.rstrip().split()
        #print parts
        if not parts: break
        index = len(tokens)
        token = parts[1]
        tag = parts[3]
        parent = int(parts[6])
        if token.startswith('#'):
            tokens.append(Symbol(tag, var=int(token[1:])))
        else:
            tokens.append(Symbol(tag, token))
        children.setdefault(parent, set()).add(index)

    if len(tokens) == 1: return None

    root = TreeNode(Symbol('ROOT'), [], 0)
    schedule = []
    for child in sorted(children[0]):
        schedule.append((root, child))

    while schedule:
        parent, index = schedule[0]
        del schedule[0]
    
        node = TreeNode(tokens[index])
        node.order = 0
        parent.insert(node)

        for child in sorted(children.get(index, [])):
            schedule.append((node, child))
            if child < index:
                node.order += 1

    return root

_bracket_split_re = re.compile(r'([(]*)([^)/]*)(?:/([^)]*))?([)]*)')

def parse_bracketed_DT(line, insert_root=True):
    line = line.rstrip()
    if not line or line == 'NULL': return None
    #print line

    root = TreeNode(Symbol('ROOT'))
    stack = [root]
    for part in line.rstrip().split():
        m = _bracket_split_re.match(part)

        for c in m.group(1):
            node = TreeNode(Symbol(None))
            stack[-1].insert(node)
            stack.append(node)

        if m.group(3) != None:
            if m.group(2).startswith('#'):
                stack[-1].data.variable = int(m.group(2)[1:])
            else:
                stack[-1].data.token = m.group(2)
            stack[-1].data.tag = m.group(3)
        else:
            stack[-1].data.tag = m.group(2)
        stack[-1].order = len(stack[-1].children)
        # FIXME: also check for vars

        for c in m.group(4):
            stack.pop()

    assert len(stack) == 1
    if not insert_root or root.children[0].data.tag == 'ROOT':
        return root.children[0]
    else:
        return root

_bracket_split_notag_re = re.compile(r'([(]*)([^)/]*)([)]*)')

def parse_bracketed_untagged_DT(line):
    line = line.rstrip()
    if not line or line == 'NULL': return None

    root = TreeNode(Symbol('TOP'))
    stack = [root]
    for part in line.rstrip().split():
        m = _bracket_split_notag_re.match(part)

        for c in m.group(1):
            node = TreeNode(Symbol(None))
            stack[-1].insert(node)
            stack.append(node)

        if stack[-1].data.token == None:
            stack[-1].data.token = m.group(2)
            stack[-1].order = len(stack[-1].children)
        else:
            child = TreeNode(Symbol(nonterm=None, term=m.group(2)))
            stack[-1].insert(child)

        for c in m.group(3):
            stack.pop()

    return root.children[0]
