/*
 * Anope IRC Services
 *
 * Copyright (C) 2011-2016 Anope Team <team@anope.org>
 *
 * This file is part of Anope. Anope is free software; you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */

#include "module.h"

struct Rewrite
{
	Anope::string client, source_message, target_message, desc;

	bool Matches(const std::vector<Anope::string> &message)
	{
		std::vector<Anope::string> sm;
		spacesepstream(this->source_message).GetTokens(sm);

		for (unsigned i = 0; i < sm.size(); ++i)
			if (i >= message.size() || (sm[i] != "$" && !sm[i].equals_ci(message[i])))
				return false;

		return true;
	}

	Anope::string Process(CommandSource &source, const std::vector<Anope::string> &params)
	{
		spacesepstream sep(this->target_message);
		Anope::string token, message;

		while (sep.GetToken(token))
		{
			if (token[0] != '$')
				message += " " + token;
			else if (token == "$me")
				message += " " + source.GetNick();
			else
			{
				int num = -1, end = -1;
				try
				{
					Anope::string num_str = token.substr(1);
					size_t hy = num_str.find('-');
					if (hy == Anope::string::npos)
					{
						num = convertTo<int>(num_str);
						end = num + 1;
					}
					else
					{
						num = convertTo<int>(num_str.substr(0, hy));
						if (hy == num_str.length() - 1)
							end = params.size();
						else
							end = convertTo<int>(num_str.substr(hy + 1)) + 1;
					}
				}
				catch (const ConvertException &)
				{
					continue;
				}

				for (int i = num; i < end && static_cast<unsigned>(i) < params.size(); ++i)
					message += " " +  params[i];
			}
		}

		message.trim();
		return message;
	}

	static std::vector<Rewrite> rewrites;

	static Rewrite *Find(const Anope::string &client, const Anope::string &cmd)
	{
		for (unsigned i = 0; i < rewrites.size(); ++i)
		{
			Rewrite &r = rewrites[i];

			if ((client.empty() || r.client.equals_ci(client)) && (r.source_message.equals_ci(cmd) || r.source_message.find_ci(cmd + " ") == 0))
				return &r;
		}

		return NULL;
	}

	static Rewrite *Match(const Anope::string &client, const std::vector<Anope::string> &params)
	{
		for (unsigned i = 0; i < rewrites.size(); ++i)
		{
			Rewrite &r = rewrites[i];

			if ((client.empty() || r.client.equals_ci(client)) && r.Matches(params))
				return &r;
		}

		return NULL;
	}
};

std::vector<Rewrite> Rewrite::rewrites;

class RewriteCommand : public Command
{
 public:
	RewriteCommand(Module *creator) : Command(creator, "rewrite", 0, 0) { }

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		std::vector<Anope::string> full_params = params;
		full_params.insert(full_params.begin(), source.command);

		Rewrite *r = Rewrite::Match(!source.c ? source.service->nick : "", full_params);
		if (r != NULL)
		{
			Anope::string new_message = r->Process(source, full_params);
			logger.Debug("Rewrote '{0}' to '{1}' using '{2}'", source.command + (!params.empty() ? " " + params[0] : ""), new_message, r->source_message);
			source.service = ServiceBot::Find(r->client, true);
			if (!source.service)
				return;
			Command::Run(source, new_message);
		}
		else
		{
			logger.Log("Unable to rewrite '{0}'", source.command + (!params.empty() ? " " + params[0] : ""));
		}
	}

	void OnServHelp(CommandSource &source) override
	{
		Rewrite *r = Rewrite::Find(!source.c ? source.service->nick : "", source.command);
		if (r != NULL && !r->desc.empty())
		{
			this->SetDesc(r->desc);
			Command::OnServHelp(source);
		}
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) override
	{
		Rewrite *r = Rewrite::Find(!source.c ? source.service->nick : "", source.command);
		if (r != NULL && !r->desc.empty())
		{
			source.Reply(r->desc);
			size_t sz = r->target_message.find(' ');
			source.Reply(_("This command is an alias to the command %s."), sz != Anope::string::npos ? r->target_message.substr(0, sz).c_str() : r->target_message.c_str());
			return true;
		}
		return false;
	}
};

class ModuleRewrite : public Module
{
	RewriteCommand cmdrewrite;

 public:
	ModuleRewrite(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR | EXTRA)
		, cmdrewrite(this)
	{
	}

	void OnReload(Configuration::Conf *conf) override
	{
		Rewrite::rewrites.clear();

		for (int i = 0; i < conf->CountBlock("command"); ++i)
		{
			Configuration::Block *block = conf->GetBlock("command", i);

			if (!block->Get<bool>("rewrite"))
				continue;

			Rewrite rw;

			rw.client = block->Get<Anope::string>("service");
			rw.source_message = block->Get<Anope::string>("rewrite_source");
			rw.target_message = block->Get<Anope::string>("rewrite_target");
			rw.desc = block->Get<Anope::string>("rewrite_description");

			if (rw.client.empty() || rw.source_message.empty() || rw.target_message.empty())
				continue;

			Rewrite::rewrites.push_back(rw);
		}
	}
};

MODULE_INIT(ModuleRewrite)
