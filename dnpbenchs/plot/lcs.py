import math
import subprocess
import sqlite3
import pandas
from scipy import stats
import plotly.graph_objects as go
import plotly.io as pio

machine = "ito"
# machine = "wisteria-o"

pio.templates.default = "plotly_white"

# https://personal.sron.nl/~pault/#sec:qualitative
colors = ["#4477AA", "#EE6677", "#228833", "#CCBB44", "#66CCEE", "#AA3377", "#BBBBBB"]

dashes = ["solid", "dashdot", "dash", "dot", "longdash", "longdashdot"]

linewidth = 2
itemwidth = 70
markersize = 14

n_runs = 3
# n_runs = 10

m_cutoff_g = 512
onecore_n = 65536

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
    isola_path = subprocess.getoutput("isola where dnpbenchs:{}".format(isola_name))
    db_path = isola_path + "/dnpbenchs/result.db"
    return sqlite3.connect(db_path)

def get_n_nodes(nodes):
    if type(nodes) is str:
        (x, y, z) = (int(n) for n in nodes.split("-")[0].split("x"))
        return x * y * z
    else:
        return nodes

def get_n_cores_per_node():
    if machine == "ito":
        return 36
    elif machine == "wisteria-o":
        return 48
    else:
        return 1

def get_raw_result(n_input, m_cutoff, measure_config, madm_build, nodes, **opts):
    isola_name = "lcs_{}_{}_{}_madm_{}_node_{}:{}".format(n_input, m_cutoff, measure_config, madm_build, nodes, opts.get("version", "latest"))
    print("loading {}...".format(isola_name))
    conn = get_db_conn(isola_name)

    n_nodes = get_n_nodes(nodes)

    if measure_config == "serial" or measure_config == "onecore":
        np = n_nodes
    elif measure_config == "allcore":
        np = n_nodes * get_n_cores_per_node()

    # get the last `n_runs` samples
    df = pandas.read_sql_query(sql="select np,time from {} where np = {} and n = {} and cutoff = {} order by i desc limit {}".format(opts.get("tablename", "dnpbenchs"), np, n_input, m_cutoff, n_runs), con=conn)
    print(len(df))

    return df

def plot_bounds(fig, n_input, m_cutoff, nodes_list, **opts):
    n_samples = 10000
    margin_left  = 1.2
    margin_right = 0.3

    df = get_raw_result(onecore_n, m_cutoff, "onecore", "future_multi", 1, **opts)
    time_onecore = df[df["np"] == 1].mean()["time"] / 1000 / 1000 / 1000
    print(time_onecore)

    n_tasks_onecore = (onecore_n / m_cutoff) ** 2
    time_per_task = time_onecore / n_tasks_onecore

    n_tasks = (n_input / m_cutoff) ** 2
    n_tasks_crit = (n_input / m_cutoff) * 2 - 1

    work = time_per_task * n_tasks
    span = time_per_task * n_tasks_crit
    parallelism = work / span

    lower_bound_fn = lambda np: work / np if np < parallelism else span
    upper_bound_fn = lambda np: work / np + span

    x_min = get_n_nodes(nodes_list[0] ) * get_n_cores_per_node()
    x_max = get_n_nodes(nodes_list[-1]) * get_n_cores_per_node()
    x_min = 10 ** (math.log10(x_min) - margin_left)
    x_max = 10 ** (math.log10(x_max) + margin_right)
    xs = [x_min + (x_max - x_min) / n_samples * n for n in range(n_samples)]

    fig.add_trace(go.Scatter(x=xs,
                             y=[lower_bound_fn(x) for x in xs],
                             mode="lines",
                             marker_color=opts.get("color", None),
                             line=dict(dash=opts.get("dash", None), width=linewidth),
                             showlegend=False))

    fig.add_trace(go.Scatter(x=xs,
                             y=[upper_bound_fn(x) for x in xs],
                             mode="lines",
                             marker_color=opts.get("color", None),
                             line=dict(dash=opts.get("dash", None), width=linewidth),
                             showlegend=False))

    # fig.add_annotation(x=math.log10(xs[-1]),
    #                    y=math.log10(lower_bound_fn(xs[-1])),
    #                    text=opts.get("title", "N={}, M={}".format(n_input, m_cutoff)),
    #                    showarrow=False,
    #                    xanchor="right",
    #                    yanchor="top",
    #                    xshift=0.1,
    #                    yshift=-1)

def plot_line(fig, n_input, m_cutoff, nodes_list, **opts):
    plot_bounds(fig, n_input, m_cutoff, nodes_list, **opts)

    result_dfs = [get_raw_result(n_input, m_cutoff, "allcore", "future_multi", n, **opts) for n in nodes_list]
    df = pandas.concat(result_dfs)

    df = df.groupby("np").agg({"time": ["mean", ci_lower, ci_upper]})
    sec_mean  = df[("time", "mean")]     / 1000 / 1000 / 1000
    sec_upper = df[("time", "ci_upper")] / 1000 / 1000 / 1000
    sec_lower = df[("time", "ci_lower")] / 1000 / 1000 / 1000

    nps = sec_mean.index

    error_y = dict(type="data",
                   symmetric=False,
                   array=sec_upper - sec_mean,
                   arrayminus=sec_mean - sec_lower,
                   thickness=linewidth)

    marker_opts = dict(color=opts.get("color", None),
                       symbol=opts.get("marker", None),
                       line_width=linewidth,
                       line_color=opts.get("color", None),
                       size=markersize)

    fig.add_trace(go.Scatter(x=nps,
                             y=sec_mean,
                             error_y=error_y,
                             mode="markers",
                             marker=marker_opts,
                             showlegend=False))

    # dummy for legends
    fig.add_trace(go.Scatter(x=[None],
                             y=[None],
                             marker=marker_opts,
                             line=dict(dash=opts.get("dash", None), width=linewidth),
                             # name=opts.get("title", "N={}".format(n_input))))
                             name=opts.get("title", "N={}, M={}".format(n_input, m_cutoff))))

def plot_fig():
    fig = go.Figure()

    if machine == "ito":
        plot_line(fig, 67108864, m_cutoff_g, [              256], color=colors[4], dash="longdash", marker="star-diamond-open"    , title="$N=2^{{26}}$")
        plot_line(fig, 16777216, m_cutoff_g, [      16, 64, 256], color=colors[3], dash="dot"     , marker="star-triangle-up-open", title="$N=2^{{24}}$")
        plot_line(fig,  4194304, m_cutoff_g, [   4, 16, 64, 256], color=colors[2], dash="solid"   , marker="circle-open"          , title="$N=2^{{22}}$")
        plot_line(fig,  1048576, m_cutoff_g, [1, 4, 16, 64, 256], color=colors[1], dash="dash"    , marker="square-open"          , title="$N=2^{{20}}$")
        plot_line(fig,   262144, m_cutoff_g, [1, 4, 16, 64, 256], color=colors[0], dash="dashdot" , marker="diamond-open"         , title="$N=2^{{18}}$")
    elif machine == "wisteria-o":
        plot_line(fig, 67108864, m_cutoff_g, [                                             "8x9x8-mesh", "12x16x12-mesh"], color=colors[4], dash="longdash", marker="star-diamond-open"    , title="$N=2^{{26}}$")
        plot_line(fig, 16777216, m_cutoff_g, [                 "3x4x3-mesh", "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], color=colors[3], dash="dot"     , marker="star-triangle-up-open", title="$N=2^{{24}}$")
        plot_line(fig,  4194304, m_cutoff_g, [   "2x3x1-mesh", "3x4x3-mesh", "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], color=colors[2], dash="solid"   , marker="circle-open"          , title="$N=2^{{22}}$")
        plot_line(fig,  1048576, m_cutoff_g, [1, "2x3x1-mesh", "3x4x3-mesh", "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], color=colors[1], dash="dash"    , marker="square-open"          , title="$N=2^{{20}}$")
        plot_line(fig,   262144, m_cutoff_g, [1, "2x3x1-mesh", "3x4x3-mesh", "6x6x4-mesh", "8x9x8-mesh", "12x16x12-mesh"], color=colors[0], dash="dashdot" , marker="diamond-open"         , title="$N=2^{{18}}$")

    log_labels_x = ["1", "10", "100", "1K", "10K", "100K", "1M", "10M", "100M", "1G", "10G", "100G"]
    log_labels_y = ["1", "10", "100", "1,000"]
    fig.update_xaxes(
        showline=True,
        linecolor='black',
        ticks="outside",
        title_text="# of processes",
        title_standoff=10,
        type="log",
        tickvals=[(10 ** i) * j for i in range(0, len(log_labels_x)) for j in range(1, 10)],
        ticktext=[i if j == 1 else " " for i in log_labels_x for j in range(1, 10)],
        tickangle=0,
    )
    fig.update_yaxes(
        showline=True,
        linecolor='black',
        ticks="outside",
        title_text="Execution time (s)",
        title_standoff=10,
        type="log",
        tickvals=[(10 ** i) * j for i in range(0, len(log_labels_y)) for j in range(1, 10)],
        ticktext=[i if j == 1 else " " for i in log_labels_y for j in range(1, 10)],
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
            x=0,
            itemwidth=itemwidth,
            # traceorder="reversed",
        ),
        font=dict(
            family="Linux Biolinum O, sans-serif",
            size=16,
        ),
    )
    fig.write_html("lcs_m_{}_{}.html".format(m_cutoff_g, machine), include_plotlyjs="cdn", include_mathjax="cdn", config={"toImageButtonOptions" : {"format" : "svg"}})

plot_fig()
