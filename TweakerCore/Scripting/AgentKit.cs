using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace TweakerCore.Scripting
{
    /// <summary>One skill of the kit, as kit.json lists it.</summary>
    public sealed class AgentKitSkill
    {
        public AgentKitSkill(string name, bool optional, string folder)
        {
            Name = name;
            Optional = optional;
            Folder = folder;
        }

        /// <summary>Also the folder name, in the kit and in the project (Cursor requires the two to match).</summary>
        public string Name { get; }

        /// <summary>Installed only when the user asks for it - audiosurf-binaries.</summary>
        public bool Optional { get; }

        public string Folder { get; }
    }

    /// <summary>
    /// DevTools\AgentKit as AgentSetup reads it (Tools/AgentKit/README.md is the description of record): kit.json,
    /// the skill folders and the four templates under agents\. Read once per setup or status check; nothing here
    /// writes.
    /// </summary>
    public sealed class AgentKit
    {
        public const string BlockFile = "agents-block.md";
        public const string AgentsTemplateFile = "AGENTS.md";
        public const string ClaudeTemplateFile = "CLAUDE.md";
        public const string ClaudeSkillTemplateFile = "claude-skill.md";

        private AgentKit(string root, int version, IReadOnlyList<AgentKitSkill> skills)
        {
            Root = root;
            Version = version;
            Skills = skills;
        }

        public string Root { get; }

        /// <summary>kit.json's version: what Script Dev shows, and a change of it alone is an update.</summary>
        public int Version { get; }

        public IReadOnlyList<AgentKitSkill> Skills { get; }

        /// <summary>The kit at <paramref name="root"/>, or null with <paramref name="problem"/> saying why not.</summary>
        public static AgentKit Load(string root, out string problem)
        {
            problem = null;
            var json = Path.Combine(root ?? "", "kit.json");
            if (string.IsNullOrEmpty(root) || !File.Exists(json))
            {
                problem = $"The AI agent kit is missing - there is no kit.json in {root}.";
                return null;
            }

            try
            {
                using var document = JsonDocument.Parse(File.ReadAllText(json));
                var version = document.RootElement.GetProperty("version").GetInt32();
                var skills = new List<AgentKitSkill>();
                foreach (var entry in document.RootElement.GetProperty("skills").EnumerateArray())
                {
                    var name = entry.GetProperty("name").GetString();
                    var optional = entry.TryGetProperty("optional", out var flag) && flag.GetBoolean();
                    var folder = Path.Combine(root, "skills", name ?? "");
                    if (string.IsNullOrEmpty(name) || !File.Exists(Path.Combine(folder, "SKILL.md")))
                    {
                        problem = $"The AI agent kit is damaged: skill '{name}' has no SKILL.md.";
                        return null;
                    }
                    skills.Add(new AgentKitSkill(name, optional, folder));
                }

                foreach (var template in new[] { BlockFile, AgentsTemplateFile, ClaudeTemplateFile, ClaudeSkillTemplateFile })
                {
                    if (!File.Exists(Path.Combine(root, "agents", template)))
                    {
                        problem = $"The AI agent kit is damaged: agents\\{template} is missing.";
                        return null;
                    }
                }

                return new AgentKit(root, version, skills);
            }
            catch (Exception exception) when (exception is JsonException || exception is KeyNotFoundException || exception is InvalidOperationException || exception is IOException)
            {
                problem = "The AI agent kit's kit.json cannot be read: " + exception.Message;
                return null;
            }
        }

        public string Template(string file) => Normalize(File.ReadAllText(Path.Combine(Root, "agents", file)));

        /// <summary>
        /// The skill's description exactly as its front matter has it - the YAML value, quoting and all - for the
        /// Claude Code pointer, which has to carry the same text for the skill to trigger the same way.
        /// </summary>
        public static string Description(AgentKitSkill skill)
        {
            var lines = Normalize(File.ReadAllText(Path.Combine(skill.Folder, "SKILL.md"))).Split('\n');
            if (lines.Length == 0 || lines[0] != "---")
                return null;
            foreach (var line in lines.Skip(1).TakeWhile(l => l != "---"))
            {
                if (line.StartsWith("description:", StringComparison.Ordinal))
                    return line["description:".Length..].Trim();
            }
            return null;
        }

        /// <summary>LF line ends, as the kit is written and as the project gets it.</summary>
        internal static string Normalize(string text) => text.Replace("\r\n", "\n");
    }
}
