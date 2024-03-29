import ast
import astunparse

class GenericVisitor(ast.NodeVisitor):
    def __init__(self):
        self.last_defined_fn = ""
        self.last_fn_args = []
        self.global_vars = []
        self.vars_fns = []
        self.fn_fns = []
        self.mutex_state = {}
        self.last_mutex = ""
        self.mutex_counter = 0

    def visit_Call(self, node):
        if isinstance(node.func, ast.Attribute):
            func_name = node.func.attr
            # print("funcname", func_name)
            if isinstance(node.func.value, ast.Name):
                func_attr = node.func.value.id
                state = 0
                if func_name == "acquire" or func_name == "release":
                    if func_name == "acquire": 
                        state = 1
                        self.last_mutex = func_attr

                    elif func_name == "release": 
                        self.last_mutex = ""
                    if func_attr in self.mutex_state:
                        self.mutex_state[func_attr] = state
                    else:
                        self.mutex_state[func_attr] = state
                        self.mutex_counter += 1
                    
        # print(self.mutex_state)
        self.generic_visit(node)


    def visit_FunctionDef(self, node):
        self.last_defined_fn = node.name
        self.last_fn_args = [a.arg for a in node.args.args]
        self.generic_visit(node)

    def visit_Global(self, node):
        for name in node.names:
            if name not in self.global_vars:
                self.global_vars.append(name)

    def visit_Assign(self, node):
        if isinstance(node.targets[0], ast.Name) and isinstance(node.value, ast.Call):
            if isinstance(node.value.func, ast.Name):
                fn_name = node.value.func.id
                if fn_name == "Thread":
                    fn_arg = node.value.keywords[0].value.id
                    if len(self.fn_fns) <= 0:
                        self.fn_fns.append(("", self.last_defined_fn))
                    self.fn_fns.append((self.last_defined_fn, fn_arg))


        for sub_node_targets in node.targets:
            for sub_node in ast.walk(sub_node_targets):
                if isinstance(sub_node, ast.Name):
                    if self.last_mutex in self.mutex_state:
                        mutex_state = self.mutex_state[self.last_mutex]
                    else:
                        mutex_state = None
                    mutex_id = 0
                    if mutex_state == 1:
                        mutex_id = list(self.mutex_state.keys()).index(self.last_mutex) + 1  
                    if sub_node.id in self.global_vars or sub_node.id in self.last_fn_args:
                        print(ast.dump(node))
                        self.vars_fns.append((self.last_defined_fn, sub_node.id, mutex_id))
        self.generic_visit(node)

    def visit_AugAssign(self, node):
        for sub_node in ast.walk(node.target):
            if isinstance(sub_node, ast.Name):
                if self.last_mutex in self.mutex_state:
                    mutex_state = self.mutex_state[self.last_mutex]
                else:
                    mutex_state = None
                mutex_id = 0
                if mutex_state == 1:
                    mutex_id = list(self.mutex_state.keys()).index(self.last_mutex) + 1
                # print(self.last_defined_fn, self.last_fn_args)
                if sub_node.id in self.global_vars or sub_node.id in self.last_fn_args or self.last_defined_fn == "main":
                    self.vars_fns.append((self.last_defined_fn, sub_node.id, mutex_id))
                    print("augassign in", self.last_defined_fn, "with name", sub_node.id, "and mutex", mutex_id)

        self.generic_visit(node)

class CodeToSicclConverter():
    def __init__(self):
        self.fn_counter = 0
        self.var_counter = 0
        self.mutex_counter = 0
        self.known_fn = {}
        self.known_mutexe = {}
        self.known_vars = {}

    def remove_non_shared_vars(siccl_array):
        shared_count: dict[int, [int, int]] = {}

        last_thread_name = 0
        for i, elem in enumerate(siccl_array):
            if elem[2] in shared_count:
                if elem[1] != shared_count[elem[2]][0]:
                    shared_count[elem[2]][1] += 1
            else:
                shared_count[elem[2]] = [elem[1], 0]
            last_thread_name = elem[1]

        for i, elem in enumerate(siccl_array):
            if shared_count[elem[2]][1] < 1:
                del(siccl_array[i])

        per_thread_count: dict[int, int] = {}
        last_thread_name = ""
        for i, elem in enumerate(siccl_array):
            if last_thread_name != elem[1]:
                per_thread_count.clear()
            if elem[1] in per_thread_count:
                per_thread_count[elem[1]] += 1
            else:
                per_thread_count[elem[1]] = 1

            if per_thread_count[elem[1]] > 1:
                del(siccl_array[i])
            last_thread_name = elem[1]
        return siccl_array

    def generate_siccle_arr(self, vars_fns, fn_fns):
        siccl_array_tok = []
        for i_fns, fn in enumerate(fn_fns):
            calling_fn_id = 0
            if fn[0] in self.known_fn:
                calling_fn_id = self.known_fn[fn[0]]
            else:
                calling_fn_id = self.fn_counter
                self.known_fn[fn[0]] = self.fn_counter
                self.fn_counter += 1

            called_fn_id = 0
            if fn[1] in self.known_fn:
                called_fn_id = self.known_fn[fn[1]]
            else:
                called_fn_id = self.fn_counter
                self.known_fn[fn[1]] = self.fn_counter
                self.fn_counter += 1

            var_id = 0
            mutex_id = 0
            for var in vars_fns:
                if var[0] == fn[1]:
                    if var[1] in self.known_vars:
                        var_id = self.known_vars[var[1]]
                    else:
                        var_id = self.var_counter
                        self.known_vars[var[1]] = self.var_counter
                        self.var_counter += 1

                    if var[2] in self.known_mutexe:
                        mutex_id = self.known_mutexe[var[2]]
                    else:
                        mutex_id = self.mutex_counter
                        self.known_mutexe[var[2]] = self.mutex_counter
                        self.mutex_counter += 1
                    
                    siccl_array_tok.append([calling_fn_id, called_fn_id, var_id, mutex_id])
                    # siccl_array_tok.append([fn[0], fn[1], var[1], var[2]])
        # siccl_arr = remove_non_shared_vars(siccl_array_tok)
        return siccl_array_tok

    def convert(self, text):
        # print(text)
        tree = ast.parse(text)
        # print(ast.dump(tree))
        visitor = GenericVisitor()
        visitor.visit(tree)
        print(visitor.vars_fns, visitor.fn_fns)
        return self.generate_siccle_arr(visitor.vars_fns, visitor.fn_fns)