/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <cassert>
#include <cstdlib>

#include <util/cmdline.h>
#include <sstream>
#include <util/message/format.h>

/* Parses 's' according to a simple interpretation of shell rules, taking only
 * whitespace and the characters ', " and \ into account. */
static std::vector<std::string>
simple_shell_unescape(const char *s, const messaget &msg, const char *var)
{
  static const char WHITE[] = " \t\r\n\f\v";

  if(!s)
    return {};
  std::vector<std::string> split;
  while(*s)
  {
    /* skip white-space */
    while(*s && strchr(WHITE, *s))
      s++;
    if(!*s)
      break;
    std::string arg;
    enum : char
    {
      NONE,
      DQUOT = '"',
      SQUOT = '\'',
      ESC = '\\',
    } mode = NONE;
    while(*s)
    {
      switch(mode)
      {
      case NONE:
        /* white-space delimits strings */
        if(strchr(WHITE, *s))
          goto done;
        /* special chars in this mode */
        switch(*s)
        {
        case '\'':
          mode = SQUOT;
          s++;
          continue;
        case '"':
          mode = DQUOT;
          s++;
          continue;
        case '\\':
          /* skip first backslash */
          mode = ESC;
          s++;
          if(!*s)
            goto done;
          mode = NONE;
          break;
        }
        break;
      case SQUOT:
        /* the only special char in single-quote mode is ' */
        switch(*s)
        {
        case '\'':
          mode = NONE;
          s++;
          continue;
        }
        break;
      case DQUOT:
        /* special chars in double-quote mode */
        switch(*s)
        {
        case '"':
          mode = NONE;
          s++;
          continue;
        case '\\':
          mode = ESC;
          if(!s[1])
            goto done;
          mode = DQUOT;
          if(strchr("\\\"", s[1]))
            s++;
          break;
        }
        break;
      case ESC:
        msg.error(fmt::format("Internal error parsing {}", var));
        abort();
      }
      arg.push_back(*s++);
    }
  done:
    if(mode)
    {
      msg.warning(fmt::format(
        "cannot parse environment variable {}: unfinished {}, ignoring...",
        var,
        mode));
      return {};
    }
    split.emplace_back(std::move(arg));
  }
  return split;
}

void cmdlinet::clear()
{
  vm.clear();
  args.clear();
  options_map.clear();
}

bool cmdlinet::isset(const char *option) const
{
  return vm.count(option) > 0;
}

const std::list<std::string> &cmdlinet::get_values(const char *option) const
{
  cmdlinet::options_mapt::const_iterator value = options_map.find(option);
  assert(value != options_map.end());
  return value->second;
}

const char *cmdlinet::getval(const char *option) const
{
  cmdlinet::options_mapt::const_iterator value = options_map.find(option);
  if(value == options_map.end())
    return nullptr;
  if(value->second.empty())
    return nullptr;
  return value->second.front().c_str();
}

bool cmdlinet::parse(
  int argc,
  const char **argv,
  const struct group_opt_templ *opts)
{
  using namespace boost;
  using namespace program_options;
  using std::string;
  using std::vector;

  clear();

  unsigned int i = 0;
  for(; opts[i].groupname != "end"; i++)
  {
    options_description op_desc(opts[i].groupname);
    for(const opt_templ &o : opts[i].options)
    {
      if(!o.type_default_value)
        op_desc.add_options()(o.optstring, o.description);
      else
        op_desc.add_options()(o.optstring, o.type_default_value, o.description);
    }
    cmdline_options.add(op_desc);
  }

  const vector<opt_templ> &hidden_group_options = opts[i + 1].options;
  options_description hidden_opts;
  for(const opt_templ &o : hidden_group_options)
  {
    if(o.optstring[0] == '\0')
      break;
    if(!o.type_default_value)
      hidden_opts.add_options()(o.optstring, "");
    else
      hidden_opts.add_options()(o.optstring, o.type_default_value, "");
  }

  options_description all_opts;
  all_opts.add(cmdline_options).add(hidden_opts);
  positional_options_description p;
  p.add("input-file", -1);
  try
  {
    store(
      command_line_parser(
        simple_shell_unescape(getenv("ESBMC_OPTS"), msg, "ESBMC_OPTS"))
        .options(all_opts)
        .run(),
      vm);
    store(
      command_line_parser(argc, argv).options(all_opts).positional(p).run(),
      vm);
  }
  catch(std::exception &e)
  {
    msg.error(fmt::format("ESBMC error: {}", e.what()));
    return true;
  }

  if(vm.count("input-file"))
    args = vm["input-file"].as<vector<string>>();

  for(const auto &[option_name, value] : vm)
  {
    std::list<std::string> res;
    if(const auto *v = any_cast<int>(&value.value()))
      res.emplace_front(std::to_string(*v));
    else if(const auto *v = any_cast<string>(&value.value()))
      res.emplace_front(*v);
    else if(const auto *v = any_cast<vector<int>>(&value.value()))
      for(const int &w : *v)
        res.emplace_front(std::to_string(w));
    else
    {
      const vector<string> &w = value.as<vector<string>>();
      res.assign(w.begin(), w.end());
    }
    if(auto [it, ins] = options_map.emplace(option_name, res); !ins)
      it->second = res;
  }

  for(const opt_templ &o : hidden_group_options)
  {
    if(o.optstring[0] == '\0')
      break;
    if(o.description[0] != '\0' && vm.count(o.description))
    {
      const std::list<string> &values = get_values(o.description);
      if(auto [it, ins] = options_map.emplace(o.optstring, values); !ins)
        it->second = values;
    }
  }

  return false;
}
