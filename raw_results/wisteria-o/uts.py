import os
import subprocess
import sqlite3
import pandas
from scipy import stats
import plotly.graph_objects as go
import plotly.io as pio

# machine = "ito"
machine = "wisteria-o"

pio.templates.default = "plotly_white"

# https://personal.sron.nl/~pault/#sec:qualitative
colors = ["#4477AA", "#EE6677", "#228833", "#CCBB44", "#66CCEE", "#AA3377", "#BBBBBB"]

dashes = ["solid", "dashdot", "dash", "dot", "longdash", "longdashdot"]

linewidth = 2
itemwidth = 70
markersize = 14

# n_runs = 3
n_runs = 100

def confidence_interval(data):
    return stats.t.interval(alpha=0.95,
                            loc=data.mean(),
                            df=len(data) - 1,
                            scale=stats.sem(data))

def ci_lower(data):
    return confidence_interval(data)[0]

def ci_upper(data):
    return confidence_interval(data)[1]

def get_db_conn(isola_name):
    db_path = isola_name + "/result.db"
    return sqlite3.connect(db_path)

def get_node_counts(tree):
    if tree == "T1L":
        return 102181082
    elif tree == "T1XL":
        return 1635119272
    elif tree == "T1XXL":
        return 4230646601
    elif tree == "T1WL":
        return 270751679750
    else:
        return 0

def get_n_cores_per_node():
    if machine == "ito":
        return 36
    elif machine == "wisteria-o":
        return 48
    else:
        return 1

def get_raw_result(tree, measure_config, madm_build, nodes, **opts):
    if "dbfile" in opts:
        conn = sqlite3.connect(opts["dbfile"])
    else:
        isola_name = "uts_{}_{}_madm_{}_node_{}".format(tree, measure_config, madm_build, nodes)
        print("loading {}...".format(isola_name))
        conn = get_db_conn(isola_name)

    if type(nodes) is str:
        (x, y, z) = (int(n) for n in nodes.split("-")[0].split("x"))
        n_nodes = x * y * z
    else:
        n_nodes = nodes

    if measure_config == "serial" or measure_config == "onecore":
        np = n_nodes
    elif measure_config == "allcore":
        np = n_nodes * get_n_cores_per_node()

    # get the last `n_runs` samples
    df = pandas.read_sql_query(sql="select np,time,nodes from {} where np = {} and nodes = {} order by i desc limit {}".format(opts.get("tablename", "dnpbenchs"), np, get_node_counts(tree), n_runs), con=conn)
    print(len(df))

    return df

def plot_line(fig, tree, nodes_list, plot_serial, **opts):
    result_dfs = [get_raw_result(tree, "allcore", "greedy_gc", n, **opts) for n in nodes_list]
    if plot_serial:
        result_dfs.append(get_raw_result(tree, "onecore", "greedy_gc", 1, **opts))
    df = pandas.concat(result_dfs)

    nc = get_node_counts(tree)

    df = df.groupby("np").agg({"time": ["mean", ci_lower, ci_upper]})
    gnodes_per_sec = nc / df[("time", "mean")]     * 1000 * 1000 * 1000
    gnps_upper     = nc / df[("time", "ci_lower")] * 1000 * 1000 * 1000
    gnps_lower     = nc / df[("time", "ci_upper")] * 1000 * 1000 * 1000

    print(df[("time", "mean")], gnodes_per_sec)

    nps = gnodes_per_sec.index

    error_y = dict(type="data",
                   symmetric=False,
                   array=gnps_upper - gnodes_per_sec,
                   arrayminus=gnodes_per_sec - gnps_lower,
                   thickness=linewidth)

    fig.add_trace(go.Scatter(x=nps,
                             y=gnodes_per_sec,
                             error_y=error_y,
                             marker_color=opts.get("color", None),
                             marker_symbol=opts.get("marker", None),
                             marker_line_width=linewidth,
                             marker_line_color=opts.get("color", None),
                             marker_size=markersize,
                             line=dict(dash=opts.get("dash", None), width=linewidth),
                             name=opts.get("title", tree)))

def plot_ideal_line(fig, tree, max_x, **opts):
    df = get_raw_result(tree, "serial", "greedy_gc", 1)
    # df = get_raw_result(tree, "onecore", "greedy_gc", 1)

    nc = get_node_counts(tree)

    gnodes_per_sec = nc / df["time"].mean() * 1000 * 1000 * 1000

    print(df["time"].mean(), gnodes_per_sec)

    fig.add_trace(go.Scatter(x=[0, max_x],
                             y=[0, max_x * gnodes_per_sec],
                             marker_color="#999999",
                             mode="lines",
                             showlegend=False,
                             line=dict(dash="2px,2px", width=1),
                             name=opts.get("title", "Ideal")))

def plot_fig(runtime):
    fig = go.Figure()

    # color0_0 = "#d7a768"
    # color0_1 = "#975c41"
    # color0_2 = "#501a32"
    # color1_0 = "#d886e6"
    # color1_1 = "#8e48bc"
    # color1_2 = "#2e1c8b"
    color0_0 = "#6699CC"
    color0_1 = "#4477AA"
    color0_2 = "#004488"
    color1_0 = "#EE99AA"
    color1_1 = "#EE6677"
    color1_2 = "#994455"
    color2_0 = "#EECC66"
    color2_1 = "#CCBB44"
    color2_2 = "#997700"
    color3_0 = "#ACD39E"
    color3_1 = "#5AAE61"
    color3_2 = "#1B7837"

    # plot_core1 = True
    plot_core1 = False

    if machine == "ito":
        plot_ideal_line(fig, "T1L", 20000)

        if runtime == "madm":
            plot_line(fig, "T1WL" , [          64, 256], False     , color=color0_2, dash="dashdot", marker="diamond-open", title="Ours (T1WL)")
            plot_line(fig, "T1XXL", [   4, 16, 64, 256], False     , color=color0_1, dash="dash"   , marker="square-open" , title="Ours (T1XXL)")
            plot_line(fig, "T1L"  , [1, 4, 16, 64, 256], plot_core1, color=color0_0, dash="solid"  , marker="circle-open" , title="Ours (T1L)")
        elif runtime == "saws":
            saws_result_file = os.path.expanduser("saws/result.db")
            plot_line(fig, "T1WL" , [          64, 256], False     , dbfile=saws_result_file, tablename="saws_uts", color=color1_2, dash="dashdot", marker="hash"      , title="SAWS (T1WL)")
            plot_line(fig, "T1XXL", [   4, 16, 64, 256], False     , dbfile=saws_result_file, tablename="saws_uts", color=color1_1, dash="dash"   , marker="x-thin"    , title="SAWS (T1XXL)")
            plot_line(fig, "T1L"  , [1, 4, 16, 64, 256], plot_core1, dbfile=saws_result_file, tablename="saws_uts", color=color1_0, dash="solid"  , marker="cross-thin", title="SAWS (T1L)")
        elif runtime == "charm":
            charm_result_file = os.path.expanduser("charm/result.db")
            plot_line(fig, "T1WL" , [          64, 256], False     , dbfile=charm_result_file, tablename="charm_uts", color=color2_2, dash="dashdot", marker="star-diamond-open"      , title="Charm++ (T1WL)")
            plot_line(fig, "T1XXL", [   4, 16, 64, 256], False     , dbfile=charm_result_file, tablename="charm_uts", color=color2_1, dash="dash"   , marker="star-triangle-up-open"  , title="Charm++ (T1XXL)")
            plot_line(fig, "T1L"  , [1, 4, 16, 64, 256], plot_core1, dbfile=charm_result_file, tablename="charm_uts", color=color2_0, dash="solid"  , marker="star-triangle-down-open", title="Charm++ (T1L)")
        elif runtime == "x10":
            x10_result_file = os.path.expanduser("x10/result.db")
            plot_line(fig, "T1WL" , [          64, 256], False     , dbfile=x10_result_file, tablename="x10_uts", color=color3_2, dash="dashdot", marker="y-up"  , title="X10 (T1WL)")
            plot_line(fig, "T1XXL", [   4, 16, 64, 256], False     , dbfile=x10_result_file, tablename="x10_uts", color=color3_1, dash="dash"   , marker="y-down", title="X10 (T1XXL)")
            plot_line(fig, "T1L"  , [1, 4, 16, 64, 256], plot_core1, dbfile=x10_result_file, tablename="x10_uts", color=color3_0, dash="solid"  , marker="y-left", title="X10 (T1L)")
    elif machine == "wisteria-o":
        plot_ideal_line(fig, "T1L", 300000)

        plot_line(fig, "T1WL" , [                               "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], False     , color=color0_2, dash="dashdot", marker="diamond-open", title="Ours (T1WL)")
        plot_line(fig, "T1XXL", [   "2x3x1-mesh", "3x4x3-mesh", "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], False     , color=color0_1, dash="dash"   , marker="square-open" , title="Ours (T1XXL)")
        plot_line(fig, "T1L"  , [1, "2x3x1-mesh", "3x4x3-mesh", "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], plot_core1, color=color0_0, dash="solid"  , marker="circle-open" , title="Ours (T1L)")

    log_labels = ["1", "10", "100", "1K", "10K", "100K", "1M", "10M", "100M", "1G", "10G", "100G"]
    fig.update_xaxes(
        showline=True,
        linecolor='black',
        ticks="outside",
        title_text="# of processes",
        title_standoff=10,
        type="log",
        tickvals=[(10 ** i) * j for i in range(0, len(log_labels)) for j in range(1, 10)],
        ticktext=[i if j == 1 else " " for i in log_labels for j in range(1, 10)],
        tickangle=0,
    )
    fig.update_yaxes(
        showline=True,
        linecolor='black',
        ticks="outside",
        title_text="Throughput (nodes/s)",
        title_standoff=10,
        type="log",
        tickvals=[(10 ** i) * j for i in range(0, len(log_labels)) for j in range(1, 10)],
        ticktext=[i if j == 1 else " " for i in log_labels for j in range(1, 10)],
        tickangle=0,
    )
    fig.update_layout(
        width=350,
        height=330,
        margin=dict(
            l=0,
            r=0,
            b=0,
            t=0,
        ),
        legend=dict(
            # yanchor="bottom",
            # y=0,
            # xanchor="right",
            # x=1,
            yanchor="top",
            y=1.02,
            xanchor="left",
            x=0.02,
            itemwidth=itemwidth,
            # traceorder="reversed",
        ),
        font=dict(
            family="Linux Biolinum O, sans-serif",
            size=16,
        ),
    )
    fig.write_html("uts_{}_{}.html".format(runtime, machine), include_plotlyjs="cdn", config={"toImageButtonOptions" : {"format" : "svg"}})

for runtime in ["madm"]:
    plot_fig(runtime)
