#include "tool_job.h"
#include <algorithm>
#include <stdexcept>
namespace pgjr
{
std::string nameError(const std::string &name)
{
    if (name.empty() || name.find_first_not_of(" \t\r\n") == std::string::npos)
        return "A name is required";
    if (name.size() > 63)
        return "Names must fit in 63 UTF-8 bytes";
    for (unsigned char c : name)
        if (c < 32 || c == 127)
            return "Control character in name";
    return {};
}
namespace
{
void valid(const std::string &name)
{
    auto error = nameError(name);
    if (!error.empty())
        throw std::runtime_error(error);
}
} // namespace
std::string maintenanceSql(Task task, const std::string &database, const std::string &schema,
                           const std::string &relation, bool analyze, bool concurrent)
{
    valid(database);
    if (schema.empty() != relation.empty())
        throw std::runtime_error("Select both schema and relation");
    std::string target;
    if (!relation.empty())
    {
        valid(schema);
        valid(relation);
        target = quoteIdentifier(schema) + "." + quoteIdentifier(relation);
    }
    if (task == Task::Vacuum)
        return "VACUUM (VERBOSE" + std::string(analyze ? ", ANALYZE" : "") + ")" +
               (target.empty() ? "" : " " + target) + ";";
    if (task == Task::Analyze)
        return "ANALYZE (VERBOSE)" + (target.empty() ? "" : " " + target) + ";";
    if (task == Task::Reindex)
        return "REINDEX (VERBOSE) " + std::string(target.empty() ? "DATABASE " : "TABLE ") +
               (concurrent ? "CONCURRENTLY " : "") + (target.empty() ? quoteIdentifier(database) : target) +
               ";";
    throw std::runtime_error("Select a maintenance operation");
}
std::string roleSql(const Json &desired, const Json &original)
{
    auto name = desired.value("name", "");
    valid(name);
    if (name.compare(0, 3, "pg_") == 0)
        throw std::runtime_error("System roles cannot be edited here");
    bool create = original.empty();
    if (!create && name != original.value("name", ""))
        throw std::runtime_error("Role renaming is not supported");
    std::string options, sql;
    for (auto key : {"login", "inherit", "createdb", "createrole", "superuser", "replication", "bypassrls"})
    {
        bool value = desired.value(key, std::string(key) == "inherit");
        if (value != original.value(key, std::string(key) == "inherit"))
        {
            std::string option = std::string(key) == "superuser" ? "SUPERUSER" : key;
            std::transform(option.begin(), option.end(), option.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            options += (value ? " " : " NO") + option;
        }
    }
    int limit = desired.value("connection_limit", -1);
    if (limit < -1)
        throw std::runtime_error("Invalid connection limit");
    if (limit != original.value("connection_limit", -1))
        options += " CONNECTION LIMIT " + std::to_string(limit);
    auto until = desired.value("valid_until", "");
    if (until != original.value("valid_until", ""))
        options += " VALID UNTIL " + quoteLiteral(until.empty() ? "infinity" : until);
    auto secret = desired.value("password", "");
    if (!secret.empty())
        options += " PASSWORD " + quoteLiteral(secret);
    else if (desired.value("clear_password", false))
        options += " PASSWORD NULL";
    if (create || !options.empty())
        sql = (create ? "CREATE ROLE " : "ALTER ROLE ") + quoteIdentifier(name) +
              (options.empty() ? "" : " WITH" + options) + ";\n";
    auto comment = desired.value("comment", "");
    if (comment != original.value("comment", ""))
        sql += "COMMENT ON ROLE " + quoteIdentifier(name) + " IS " +
               (comment.empty() ? "NULL" : quoteLiteral(comment)) + ";\n";
    auto parents = desired.value("memberships", std::vector<std::string>{}),
         before = original.value("memberships", std::vector<std::string>{});
    for (const auto &p : parents)
    {
        valid(p);
        if (p == name)
            throw std::runtime_error("A role cannot be a member of itself");
        if (std::find(before.begin(), before.end(), p) == before.end())
            sql += "GRANT " + quoteIdentifier(p) + " TO " + quoteIdentifier(name) + ";\n";
    }
    for (const auto &p : before)
        if (std::find(parents.begin(), parents.end(), p) == parents.end())
            sql += "REVOKE " + quoteIdentifier(p) + " FROM " + quoteIdentifier(name) + ";\n";
    return sql;
}
std::string rolesQuery()
{
    return "SELECT r.rolname, r.rolcanlogin, r.rolinherit, r.rolcreatedb, r.rolcreaterole, r.rolsuper, "
           "r.rolreplication, r.rolbypassrls, r.rolconnlimit, r.rolvaliduntil, "
           "pg_catalog.shobj_description(r.oid,'pg_authid'), COALESCE((SELECT json_agg(p.rolname ORDER BY "
           "p.rolname) FROM pg_auth_members m JOIN pg_roles p ON p.oid=m.roleid WHERE m.member=r.oid),'[]') "
           "FROM pg_roles r ORDER BY r.rolname;";
}
std::string rightsQuery(const std::string &role, const std::string &database)
{
    return "SELECT p.privilege, EXISTS(SELECT 1 FROM aclexplode(COALESCE(d.datacl,acldefault('d',d.datdba))) "
           "a WHERE a.grantee=r.oid AND a.privilege_type=p.privilege), EXISTS(SELECT 1 FROM "
           "aclexplode(COALESCE(d.datacl,acldefault('d',d.datdba))) a WHERE a.grantee=r.oid AND "
           "a.privilege_type=p.privilege AND a.is_grantable), "
           "has_database_privilege(r.oid,d.oid,p.privilege), pg_get_userbyid(d.datdba) FROM pg_database d "
           "CROSS JOIN pg_roles r CROSS JOIN (VALUES ('CONNECT'),('CREATE'),('TEMPORARY')) p(privilege) "
           "WHERE r.rolname=" +
           quoteLiteral(role) + " AND d.datname=" + quoteLiteral(database) + " ORDER BY p.privilege;";
}
std::string rightsSql(const std::string &role, const std::string &database, const Json &desired,
                      const Json &original, bool owner)
{
    valid(role);
    valid(database);
    std::string sql;
    for (std::string p : {"CONNECT", "CREATE", "TEMPORARY"})
    {
        bool value = desired.value(p, false), before = original.value(p, false),
             grant = value && desired.value(p + "_grant", false), old = original.value(p + "_grant", false);
        auto target = p + " ON DATABASE " + quoteIdentifier(database);
        if (value && (!before || (grant && !old)))
            sql += "GRANT " + target + " TO " + quoteIdentifier(role) +
                   (grant ? " WITH GRANT OPTION;\n" : ";\n");
        else if (!value && before)
            sql += "REVOKE " + target + " FROM " + quoteIdentifier(role) + ";\n";
        if (value && before && old && !grant)
            sql += "REVOKE GRANT OPTION FOR " + target + " FROM " + quoteIdentifier(role) + ";\n";
    }
    if (owner && original.value("owner", "") != role)
        sql += "ALTER DATABASE " + quoteIdentifier(database) + " OWNER TO " + quoteIdentifier(role) + ";\n";
    return sql;
}
} // namespace pgjr
