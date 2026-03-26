#!/usr/bin/env python3
import re
from dataclasses import dataclass
from enum import StrEnum, auto
from pathlib import Path

from parsimonious.exceptions import ParseError
from parsimonious.grammar import Grammar
from parsimonious.nodes import NodeVisitor

# Define a simple grammar for C declarations
grammar = Grammar(
    r"""
    output = decl ";" (_ ~"//.*")?
    decl = function_decl / var_decl / function_pointer_decl
    function_decl = type _ function_name _ parameters
    var_decl = type _ var_name array_subscript (_ "=" _ ~"[^;]*")?
    function_pointer_decl = type _ ("(*" _ qualifier _ function_name _ array_subscript _ ")") _ parameters

    var_name = identifier
    function_name = identifier
    parameters = ("(" _ "void" _ ")") / ("(" _ (parameter_list?) _ ")")
    parameter_list = parameter _ ("," _ parameter _)*
    parameter = decl / type
    qualifier = ~"const|__cdecl|__stdcall|__thiscall|__fastcall"
    array_subscript = (_ "[" number? "]")*

    type = (qualifier _)? ~r"[a-zA-Z_][a-zA-Z0-9_]*" _ (qualifier _)? "*"* (qualifier _)?

    number = ~"0[0xX][0-9a-fA-F]+" / ~"[0-9]+"
    identifier = ~r"[a-zA-Z_][a-zA-Z0-9_]*"
    _ = ~r"\s*"
    """
)


class DeclarationVisitor(NodeVisitor):
    def visit_output(self, node, visited_children):
        return visited_children[0]

    def visit_decl(self, node, visited_children):
        return visited_children[0]

    def visit_function_decl(self, node, visited_children):
        return visited_children[2]

    def visit_function_pointer_decl(self, node, visited_children):
        return visited_children[2][4]

    def visit_identifier(self, node, visited_children):
        return node.text

    def visit_var_decl(self, node, visited_children):
        return visited_children[2]

    def visit_function_name(self, node, visited_children):
        return node

    def visit_var_name(self, node, visited_children):
        return node

    def generic_visit(self, node, visited_children):
        return visited_children or node


def extract_symbol_name(c_declaration: str) -> str | None:
    result: str | None
    try:
        visitor = DeclarationVisitor()
        tree = grammar.parse(c_declaration)
        result = visitor.visit(tree)
    except ParseError:
        result = None
    return result


class ProgressFileSection(StrEnum):
    TYPES = "types"
    FUNCTIONS = "functions"
    VARIABLES = "variables"


@dataclass
class Symbol:
    offset: int
    signature: str
    size: int | None = None

    @property
    def name(self) -> str:
        return extract_symbol_name(self.signature)

    @property
    def offset_str(self) -> str:
        return f"0x{self.offset:08X}"

    @property
    def is_known(self) -> bool:
        return not re.search(r"(\s|^)sub_", self.signature)


@dataclass
class ProgressFile:
    types: list[str]
    functions: list[Symbol]
    variables: list[Symbol]


def to_int(source: str) -> int | None:
    source = source.strip()
    if source.startswith("/*"):
        source = source[2:]
    if source.endswith("*/"):
        source = source[:-2]
    source = source.strip()
    if not source.replace("-", ""):
        return None
    if source.startswith(("0x", "0X")):
        source = source[2:]
    return int(source, 16)


def parse_progress_file(path: Path) -> ProgressFile:
    result = ProgressFile(types=[], functions=[], variables=[])

    type_contents = ""

    section: ProgressFileSection | None = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if match := re.match("^# ([A-Z]+)$", line.strip()):
            section_name = match.group(1).lower()
            if section_name in list(ProgressFileSection):
                section = ProgressFileSection(section_name)

        if line.strip().startswith("#"):
            continue

        if section == ProgressFileSection.TYPES:
            type_contents += line + "\n"

        line = line.strip()
        if not line:
            continue

        if section == ProgressFileSection.FUNCTIONS:
            offset, size, signature = re.split(r"\s+", line, maxsplit=2)
            result.functions.append(
                Symbol(
                    signature=signature,
                    offset=to_int(offset),
                    size=to_int(size),
                )
            )

        if section == ProgressFileSection.VARIABLES:
            offset, signature = re.split(r"\s+", line, maxsplit=1)
            result.variables.append(
                Symbol(
                    signature=signature,
                    offset=to_int(offset),
                )
            )

    result.types = [
        definition
        for definition in re.split(r"\n\n(?!\s)", type_contents, flags=re.M)
        if definition.strip()
    ]
    return result
