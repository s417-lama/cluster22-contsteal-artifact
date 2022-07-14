import subprocess
import sqlite3
import pandas

# machine = "ito"
machine = "wisteria-o"

comp_patterns = ["pfor", "rrm"]
# comp_patterns = ["pfor"]
# comp_patterns = ["rrm"]

n_warmup = 0

index_name = "Steal Strategy"

def get_db_conn(isola_name):
    db_path = isola_name + "/result.db"
    return sqlite3.connect(db_path)

def get_data(name, comp_pattern, madm_build, nodes, m, **opts):
    isola_name = "sched_{}_size_n_no_buf_madm_{}_node_{}".format(comp_pattern, madm_build, nodes)
    print("loading {}...".format(isola_name))

    conn = get_db_conn(isola_name)
    df = pandas.read_sql_query(sql="select np,n,time,join_acc,join_count,steal_success_acc,steal_success_count,steal_fail_acc,steal_fail_count,steal_task_copy_acc,steal_task_size_acc from dnpbenchs where m = {} and i >= {}".format(m, n_warmup), con=conn)
    print(len(df))

    df = df.groupby("n").agg("mean")
    df[index_name] = name

    df["Execution Time (s)"]                 = df["time"] / 1_000_000_000.0
    df["# of Outstanding Joins"]             = df["join_count"].round().astype(int)
    df["Average Outstanding Join Time (us)"] = df["join_acc"] / df["join_count"] / 1000
    df["Average Succeeded Steal Time (us)"]  = df["steal_success_acc"] / df["steal_success_count"] / 1000
    df["# of Succeeded Steals"]              = df["steal_success_count"].round().astype(int)
    df["Average Succeeded Steal Time (us)"]  = df["steal_success_acc"] / df["steal_success_count"] / 1000
    # df["Total Succeeded Steal Time / P (s)"] = df["steal_success_acc"] / df["np"] / 1_000_000_000.0
    df["# of Failed Steals"]                 = df["steal_fail_count"].round().astype(int)
    df["Average Failed Steal Time (us)"]     = df["steal_fail_acc"] / df["steal_fail_count"] / 1000
    # df["Total Failed Steal Time / P (s)"]    = df["steal_fail_acc"] / df["np"] / 1_000_000_000.0
    df["Average Stolen Task Size (bytes)"]   = (df["steal_task_size_acc"] / df["steal_success_count"]).round().astype(int)
    df["Average Task Copy Time (us)"]        = df["steal_task_copy_acc"] / df["steal_success_count"] / 1000

    df = df.drop(columns={"time", "np", "join_acc", "join_count", "steal_success_acc", "steal_fail_acc", "steal_task_size_acc", "steal_task_copy_acc", "steal_success_count", "steal_fail_count"})

    return df

def create_table(comp_pattern, nodes, m, **opts):
    df = pandas.concat([
        get_data("Continuation Stealing (greedy)"  , comp_pattern, "greedy_gc_prof"         , nodes, m, **opts),
        get_data("Continuation Stealing (stalling)", comp_pattern, "waitq_gc_prof"          , nodes, m, **opts),
        get_data("Child Stealing (Full)"           , comp_pattern, "child_stealing_ult_prof", nodes, m, **opts),
        get_data("Child Stealing (RtC)"            , comp_pattern, "child_stealing_prof"    , nodes, m, **opts),
    ])

    with open("sched_{}_prof_{}_node_{}.html".format(comp_pattern, machine, nodes), mode="w") as f:
        f.write("""
<!DOCTYPE html>
<meta charset="UTF-8"/>
<script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
<link rel="stylesheet" href="https://sindresorhus.com/github-markdown-css/github-markdown.css">
<div id="rendered" class="markdown-body"></div>
<div id="mdtxt" style="display:none;">
""")

        for n, n_df in df.groupby("n"):
            n_df = n_df.set_index(index_name)

            f.write("## N = {}\n\n".format(n))

            f.write(n_df.to_markdown())

            f.write("\n\n```\n")
            f.write(n_df.to_markdown())
            f.write("\n```\n")

            f.write("\n```\n")
            f.write(n_df.to_latex())
            f.write("```\n\n")

        f.write("""
</div>
<script>
  document.getElementById("rendered").innerHTML = marked.parse(document.getElementById("mdtxt").innerText);
</script>
""")

for cp in comp_patterns:
    if machine == "ito":
        # for nodes in [16, 256]:
        for nodes in [16]:
            create_table(cp, nodes, 10000)
    elif machine == "wisteria-o":
        # for nodes in ["3x4x3-mesh", "12x16x12-mesh"]:
        for nodes in ["3x4x3-mesh"]:
            create_table(cp, nodes, 10000)
