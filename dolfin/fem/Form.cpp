// Copyright (C) 2007-2011 Garth N. Wells
//
// This file is part of DOLFIN (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

#include "Form.h"
#include <dolfin/fem/FiniteElement.h>
#include <dolfin/function/Function.h>
#include <dolfin/function/FunctionSpace.h>
#include <dolfin/function/GenericFunction.h>
#include <dolfin/log/LogStream.h>
#include <dolfin/log/log.h>
#include <dolfin/mesh/Mesh.h>
#include <dolfin/mesh/MeshFunction.h>
#include <memory>
#include <string>

using namespace dolfin;

//-----------------------------------------------------------------------------
Form::Form(std::size_t rank, std::size_t num_coefficients)
    : _function_spaces(rank), _coefficients(num_coefficients)
{
  // Do nothing
}
//-----------------------------------------------------------------------------
Form::Form(std::shared_ptr<const ufc::form> ufc_form,
           std::vector<std::shared_ptr<const FunctionSpace>> function_spaces)
    : _ufc_form(ufc_form), _function_spaces(function_spaces),
      _coefficients(ufc_form->num_coefficients())
{
  dolfin_assert(ufc_form->rank() == function_spaces.size());
}
//-----------------------------------------------------------------------------
Form::~Form()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
std::size_t Form::rank() const { return _function_spaces.size(); }
//-----------------------------------------------------------------------------
std::size_t Form::num_coefficients() const
{
  if (!_ufc_form)
    return _coefficients.size();
  else
  {
    dolfin_assert(_ufc_form->num_coefficients() == _coefficients.size());
    return _coefficients.size();
  }
}
//-----------------------------------------------------------------------------
std::size_t Form::original_coefficient_position(std::size_t i) const
{
  dolfin_assert(_ufc_form);
  return _ufc_form->original_coefficient_position(i);
}
//-----------------------------------------------------------------------------
std::vector<std::size_t> Form::coloring(std::size_t entity_dim) const
{
  warning("Form::coloring does not properly consider form type.");

  // Get mesh
  dolfin_assert(this->mesh());
  const Mesh& mesh = *(this->mesh());
  const std::size_t cell_dim = mesh.topology().dim();

  std::vector<std::size_t> _coloring;
  if (entity_dim == cell_dim)
    _coloring = {{cell_dim, 0, cell_dim}};
  else if (entity_dim == cell_dim - 1)
    _coloring = {{cell_dim - 1, cell_dim, 0, cell_dim, cell_dim - 1}};
  else
  {
    dolfin_error("Form.cpp", "color form for multicore computing",
                 "Only cell and facet coloring are currently supported");
  }

  return _coloring;
}
//-----------------------------------------------------------------------------
void Form::set_mesh(std::shared_ptr<const Mesh> mesh) { _mesh = mesh; }
//-----------------------------------------------------------------------------
std::shared_ptr<const Mesh> Form::mesh() const
{
  // In the case when there are no function spaces (in the case of a
  // a functional) the (generated) subclass must set the mesh directly
  // by calling set_mesh().

  // Extract meshes from function spaces
  std::vector<std::shared_ptr<const Mesh>> meshes;
  for (std::size_t i = 0; i < _function_spaces.size(); i++)
  {
    if (_function_spaces[i])
    {
      dolfin_assert(_function_spaces[i]->mesh());
      meshes.push_back(_function_spaces[i]->mesh());
    }
  }

  // Add common mesh if any
  if (_mesh)
    meshes.push_back(_mesh);

  // Extract meshes from markers if any
  if (dx)
    meshes.push_back(dx->mesh());
  if (ds)
    meshes.push_back(ds->mesh());
  if (dS)
    meshes.push_back(dS->mesh());
  if (dP)
    meshes.push_back(dP->mesh());

  // Extract meshes from coefficients. Note that this is only done
  // when we don't already have a mesh sine it may otherwise conflict
  // with existing meshes (if coefficient is defined on another mesh).
  if (meshes.empty())
  {
    for (std::size_t i = 0; i < _coefficients.size(); i++)
    {
      const Function* function
          = dynamic_cast<const Function*>(&*_coefficients[i]);
      if (function && function->function_space()->mesh())
        meshes.push_back(function->function_space()->mesh());
    }
  }

  // Check that we have at least one mesh
  if (meshes.empty())
  {
    dolfin_error(
        "Form.cpp", "extract mesh from form",
        "No mesh was found. Try passing mesh to the assemble function");
  }

  // Check that all meshes are the same
  for (std::size_t i = 1; i < meshes.size(); i++)
  {
    if (meshes[i] != meshes[i - 1])
    {
      dolfin_error("Form.cpp", "extract mesh from form",
                   "Non-matching meshes for function spaces and/or measures");
    }
  }

  // Return first mesh
  dolfin_assert(meshes[0]);
  return meshes[0];
}
//-----------------------------------------------------------------------------
std::shared_ptr<const FunctionSpace> Form::function_space(std::size_t i) const
{
  dolfin_assert(i < _function_spaces.size());
  return _function_spaces[i];
}
//-----------------------------------------------------------------------------
std::vector<std::shared_ptr<const FunctionSpace>> Form::function_spaces() const
{
  return _function_spaces;
}
//-----------------------------------------------------------------------------
void Form::set_coefficient(std::size_t i,
                           std::shared_ptr<const GenericFunction> coefficient)
{
  dolfin_assert(i < _coefficients.size());
  _coefficients[i] = coefficient;
}
//-----------------------------------------------------------------------------
std::shared_ptr<const GenericFunction> Form::coefficient(std::size_t i) const
{
  dolfin_assert(i < _coefficients.size());
  return _coefficients[i];
}
//-----------------------------------------------------------------------------
std::vector<std::shared_ptr<const GenericFunction>> Form::coefficients() const
{
  return _coefficients;
}
//-----------------------------------------------------------------------------
std::string Form::coefficient_name(std::size_t i) const
{
  // Create name like "w0", overloaded by Form subclasses generated by form
  // compilers
  std::ostringstream name;
  name << "w" << i;
  return name.str();
}
//-----------------------------------------------------------------------------
std::shared_ptr<const MeshFunction<std::size_t>> Form::cell_domains() const
{
  return dx;
}
//-----------------------------------------------------------------------------
std::shared_ptr<const MeshFunction<std::size_t>>
Form::exterior_facet_domains() const
{
  return ds;
}
//-----------------------------------------------------------------------------
std::shared_ptr<const MeshFunction<std::size_t>>
Form::interior_facet_domains() const
{
  return dS;
}
//-----------------------------------------------------------------------------
std::shared_ptr<const MeshFunction<std::size_t>> Form::vertex_domains() const
{
  return dP;
}
//-----------------------------------------------------------------------------
void Form::set_cell_domains(
    std::shared_ptr<const MeshFunction<std::size_t>> cell_domains)
{
  dx = cell_domains;
}
//-----------------------------------------------------------------------------
void Form::set_exterior_facet_domains(
    std::shared_ptr<const MeshFunction<std::size_t>> exterior_facet_domains)
{
  ds = exterior_facet_domains;
}
//-----------------------------------------------------------------------------
void Form::set_interior_facet_domains(
    std::shared_ptr<const MeshFunction<std::size_t>> interior_facet_domains)
{
  dS = interior_facet_domains;
}
//-----------------------------------------------------------------------------
void Form::set_vertex_domains(
    std::shared_ptr<const MeshFunction<std::size_t>> vertex_domains)
{
  dP = vertex_domains;
}
//-----------------------------------------------------------------------------
std::shared_ptr<const ufc::form> Form::ufc_form() const { return _ufc_form; }
//-----------------------------------------------------------------------------
void Form::check() const
{
  dolfin_assert(_ufc_form);

  // Check that the number of argument function spaces is correct
  if (_ufc_form->rank() != _function_spaces.size())
  {
    dolfin_error("Form.cpp", "assemble form",
                 "Expecting %d function spaces (not %d)", _ufc_form->rank(),
                 _function_spaces.size());
  }

  // Check that the number of coefficient function spaces is correct
  if (_ufc_form->num_coefficients() != _coefficients.size())
  {
    dolfin_error("Form.cpp", "assemble form",
                 "Expecting %d coefficient (not %d)",
                 _ufc_form->num_coefficients(), _coefficients.size());
  }

  // Check argument function spaces
  for (std::size_t i = 0; i < _function_spaces.size(); ++i)
  {
    std::unique_ptr<ufc::finite_element> element(
        _ufc_form->create_finite_element(i));
    dolfin_assert(element);
    dolfin_assert(_function_spaces[i]->element());
    if (element->signature() != _function_spaces[i]->element()->signature())
    {
      log(ERROR, "Expected element: %s", element->signature());
      log(ERROR, "Input element:    %s",
          _function_spaces[i]->element()->signature().c_str());
      dolfin_error("Form.cpp", "assemble form",
                   "Wrong type of function space for argument %d", i);
    }
  }
}
//-----------------------------------------------------------------------------
