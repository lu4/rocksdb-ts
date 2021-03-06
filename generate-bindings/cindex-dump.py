#!/usr/bin/env python
import json
#===- cindex-dump.py - cindex/Python Source Dump -------------*- python -*--===#
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

from clang.cindex import CursorKind

#import CursorKind from clang.cindex.CursorKind

"""
A simple command line tool for dumping a source file using the Clang Index
Library.
"""

def get_diag_info(diag):
    return { 'severity' : diag.severity,
             'location' : diag.location,
             'spelling' : diag.spelling,
             'ranges' : diag.ranges,
             'fixits' : diag.fixits }

def get_cursor_id(cursor, cursor_list = []):
    if not opts.showIDs:
        return None

    if cursor is None:
        return None

    # FIXME: This is really slow. It would be nice if the index API exposed
    # something that let us hash cursors.
    for i,c in enumerate(cursor_list):
        if cursor == c:
            return i
    cursor_list.append(cursor)
    return len(cursor_list) - 1

def get_info(node, depth=0):
    f_args=[]
    f_result={}

    if opts.maxDepth is not None and depth >= opts.maxDepth:
        children = None
    else:
        children = [get_info(c, depth+1)
                    for c in node.get_children()]

    if node.kind == CursorKind.FUNCTION_DECL:
        for func_arg in node.get_children():
            f_args.append({
                'displayname': func_arg.displayname,
                'kind': func_arg.kind.__repr__(),
                'type': func_arg.type.kind.__repr__(),
                'spelling': func_arg.type.spelling
            })
        f_result = {
            'kind' : node.result_type.kind.__repr__(),
            'spelling': node.result_type.spelling
        }
    return { 'id' : get_cursor_id(node).__str__(),
             'kind' : node.kind.__repr__(),
             'usr' : node.get_usr(),
             'f_args' : f_args,
             'f_result': f_result,
             'spelling': node.spelling,
             'is_definition' : node.is_definition(),
             'definition id' : get_cursor_id(node.get_definition()),
             'children' : children }

def main():
    from clang.cindex import Index
    from pprint import pprint

    from optparse import OptionParser, OptionGroup

    global opts

    parser = OptionParser("usage: %prog [options] {filename} [clang-args*]")
    parser.add_option("", "--show-ids", dest="showIDs",
                      help="Compute cursor IDs (very slow)",
                      action="store_true", default=False)
    parser.add_option("", "--max-depth", dest="maxDepth",
                      help="Limit cursor expansion to depth N",
                      metavar="N", type=int, default=None)
    parser.disable_interspersed_args()
    (opts, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('invalid number arguments')

    index = Index.create()
    tu = index.parse(None, args)
    if not tu:
        parser.error("unable to load input")

   # pprint(('diags', map(get_diag_info, tu.diagnostics)))

    # print(get_info(tu.cursor));

    print(json.dumps(get_info(tu.cursor)) )

if __name__ == '__main__':
    main()

