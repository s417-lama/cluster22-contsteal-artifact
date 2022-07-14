import subprocess
import sqlite3
import numpy
import pandas
from scipy import stats
import plotly.graph_objects as go
import plotly.io as pio

machine = "ito"
# machine = "wisteria-o"

comp_patterns = ["pfor", "rrm"]
# comp_patterns = ["pfor"]
# comp_patterns = ["rrm"]

# eval_pattern = "opt"
# eval_pattern = "scalability"
# eval_pattern = "granularity"
# eval_pattern = "child_continuation"
eval_pattern = "opt+child_continuation"

pio.templates.default = "plotly_white"

# https://personal.sron.nl/~pault/#sec:qualitative
colors = ["#4477AA", "#EE6677", "#228833", "#CCBB44", "#66CCEE", "#AA3377", "#BBBBBB"]

dashes = ["solid", "dashdot", "dash", "dot", "longdash", "longdashdot"]

linewidth = 2
itemwidth = 70
markersize = 10

n_warmup = 0
# n_warmup = 2

prof_enabled = True
# prof_enabled = False

# showlegend = True
showlegend = False

# plot_only_legend = True
plot_only_legend = False

if machine == "ito":
    # nodes = 1
    nodes = 16
    # nodes = 256
elif machine == "wisteria-o":
    nodes = "3x4x3-mesh"
    # nodes = "6x6x4-mesh"
    # nodes = "12x16x12-mesh"

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

def plot_result(idx, fig, prefix, madm_build, nodes, m, **opts):
    if prof_enabled:
        madm_build = madm_build + "_prof"
    isola_name = "{}_madm_{}_node_{}:{}".format(prefix, madm_build, nodes, opts.get("version", "latest"))

    color = opts.get("color", colors[idx])
    marker = dict(
        color=color,
        symbol=opts.get("marker", None),
        line_width=linewidth,
        line_color=color,
        size=markersize,
    )

    if not plot_only_legend:
        print("loading {}...".format(isola_name))

        conn = get_db_conn(isola_name)
        df = pandas.read_sql_query(sql="select np,n,m,time,n_tasks,work,span,busy from dnpbenchs where m = {} and i >= {}".format(m, n_warmup), con=conn)
        print(len(df))

        # df = df.groupby("n").agg({"time": ["mean", "median", "min", ci_lower, ci_upper], "n_tasks": numpy.unique, "m": numpy.unique, "np": numpy.unique, "busy": "median"})
        df = df.groupby("n").agg({"time": ["mean", "median", "min", ci_lower, ci_upper], "n_tasks": "min", "m": "min", "np": "min", "busy": "median"})

        ideals = df[("n_tasks", "min")] * df[("m", "min")] / df[("np", "min")]

        # ys = (1.0 - ideals / df[("time", "mean")]) * 100
        # ci_uppers = numpy.maximum(0.0, 1.0 - ideals / df[("time", "ci_upper")]) * 100 - ys
        # ci_lowers = ys - numpy.maximum(0.0, 1.0 - ideals / df[("time", "ci_lower")]) * 100

        # ys = (df[("time", "mean")] / ideals - 1.0) * 100
        # ci_uppers = numpy.maximum(0.0, df[("time", "ci_upper")] / ideals - 1.0) * 100 - ys
        # ci_lowers = ys - numpy.maximum(0.0, df[("time", "ci_lower")] / ideals - 1.0) * 100
        # error_y = dict(type="data", symmetric=False, array=ci_uppers, arrayminus=ci_lowers, thickness=linewidth)

        ys = ideals / df[("time", "mean")] * 100
        ci_uppers = ideals / df[("time", "ci_lower")] * 100 - ys
        ci_lowers = ys - ideals / df[("time", "ci_upper")] * 100
        error_y = dict(type="data", symmetric=False, array=ci_uppers, arrayminus=ci_lowers, thickness=linewidth)

        # ys = (df[("time", "min")] / ideals - 1.0) * 100
        # error_y = dict()

        # ys = 100.0 - df[("busy", "median")]
        # error_y = dict()

        # xs = ys.index
        # xs = df[("n_tasks", "unique")]
        # xs = df[("n_tasks", "unique")] * df[("m", "unique")] / 1000 / 1000 / 1000
        xs = ideals

        fig.add_trace(go.Scatter(x=xs,
                                 y=ys,
                                 error_y=error_y,
                                 marker=marker,
                                 line=dict(dash=opts.get("dash", dashes[idx]), width=linewidth),
                                 name=opts.get("title", isola_name)))
    else:
        fig.add_trace(go.Scatter(x=[None],
                                 y=[None],
                                 marker=marker,
                                 line=dict(dash=opts.get("dash", dashes[idx]), width=linewidth),
                                 name=opts.get("title", isola_name)))

def plot_comp_pattern(comp_pattern):
    fig = go.Figure()

    if eval_pattern == "opt":
        if machine == "ito":
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq"    , nodes, 10000, title="Baseline")
            # plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy"   , nodes, 10000, title="Greedy Resuming")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000, title="Greedy Resuming + Local Collection")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc" , nodes, 10000, title="Local Collection")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000, title="Local Collection + Greedy Join")

            # plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy"   , nodes, 10000, title="Greedy Resuming")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc" , nodes, 10000, title="Local Collection")
            # plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000, title="Greedy Resuming + Local Collection")

            # plot_result(1, fig, "sched_{}_size_n_buf_10".format(comp_pattern), "waitq"    , nodes, 10000, title="Buffered Return (n=10)")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc" , nodes, 10000, title="Local Collection")
            # plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000, title="Local Collection + Greedy Resuming")
            # if comp_pattern == "pfor":
            #     plot_result(4, fig, "sched_{}_size_n_mpi".format(comp_pattern), "greedy_gc", nodes, 10000, title="Bulk Synchronous Parallel (MPI)")
        elif machine == "wisteria-o":
            # plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq"    , nodes, 10000, title="Baseline")
            # plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc" , nodes, 10000, title="Local Collection")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy"   , nodes, 10000, title="Greedy Resuming")
            # plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000, title="Local Collection + Greedy Resuming")
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq"    , nodes, 10000, title="Baseline")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc" , nodes, 10000, title="Local Collection")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000, title="Local Collection + Greedy Join")
    elif eval_pattern == "scalability":
        if machine == "ito":
            # plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  16, 10000, title="16 nodes (576 cores)")
            # plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  32, 10000, title="32 nodes (1152 cores)")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  64, 10000, title="64 nodes (2304 cores)")
            # plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", 128, 10000, title="128 nodes (4608 cores)")
            # plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", 256, 10000, title="256 nodes (9216 cores)")
            # plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",   1, 10000, title="1 node (36 cores)")
            # plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",   4, 10000, title="4 nodes (144 cores)")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  16, 10000, title="16 nodes (576 cores)")
            # plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  64, 10000, title="64 nodes (2304 cores)")
            # plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", 256, 10000, title="256 nodes (9216 cores)")
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",   1, 10000, title="36 cores")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",   4, 10000, title="144 cores")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  16, 10000, title="576 cores")
            plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",  64, 10000, title="2,304 cores")
            plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", 256, 10000, title="9,216 cores")
        elif machine == "wisteria-o":
            # plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",             "1", 10000, title="1 node (48 cores)")
            # plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "2x3x1-mesh", 10000, title="6 nodes (288 cores)")
            # plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "3x4x3-mesh", 10000, title="36 nodes (1728 cores)")
            # plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "6x6x4-mesh", 10000, title="144 nodes (6912 cores)")
            # plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "8x9x8-mesh", 10000, title="576 nodes (27648 cores)")
            # plot_result(5, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", "12x16x12-mesh", 10000, title="2304 nodes (110592 cores)")
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",             "1", 10000, title="48 cores")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "2x3x1-mesh", 10000, title="288 cores")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "3x4x3-mesh", 10000, title="1,728 cores")
            plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "6x6x4-mesh", 10000, title="6,912 cores")
            plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc",    "8x9x8-mesh", 10000, title="27,648 cores")
            plot_result(5, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc", "12x16x12-mesh", 10000, title="110,592 cores")
    elif eval_pattern == "granularity":
        if machine == "ito":
            plot_result(0, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", 16, 1000  , title="M = 1 us")
            plot_result(1, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", 16, 2000  , title="M = 2 us")
            plot_result(2, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", 16, 5000  , title="M = 5 us")
            plot_result(3, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", 16, 10000 , title="M = 10 us")
            plot_result(4, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", 16, 100000, title="M = 100 us")
        elif machine == "wisteria-o":
            plot_result(0, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", nodes, 1000  , title="M = 1 us")
            plot_result(1, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", nodes, 2000  , title="M = 2 us")
            plot_result(2, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", nodes, 5000  , title="M = 5 us")
            plot_result(3, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", nodes, 10000 , title="M = 10 us")
            plot_result(4, fig, "sched_{}_size_nm_no_buf".format(comp_pattern), "greedy_gc", nodes, 100000, title="M = 100 us")
    elif eval_pattern == "child_continuation":
        if machine == "ito":
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing"    , nodes, 10000, title="Child Stealing (RtC)")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing_ult", nodes, 10000, title="Child Stealing (Full)")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc"         , nodes, 10000, title="Continuation Stealing")
        elif machine == "wisteria-o":
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing"    , nodes, 10000, title="Child Stealing (RtC)")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing_ult", nodes, 10000, title="Child Stealing (Full)")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc"         , nodes, 10000, title="Continuation Stealing")
    elif eval_pattern == "opt+child_continuation":
        if machine == "ito":
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing"    , nodes, 10000, color="#994455", dash="longdash", marker="star-triangle-down-open", title="Child Steal (RtC)")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing_ult", nodes, 10000, color="#EE6677", dash="dash"    , marker="star-triangle-up-open"  , title="Child Steal (Full)")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq"             , nodes, 10000, color="#6699CC", dash="solid"   , marker="diamond-open"           , title="Cont. Steal (baseline)")
            plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc"          , nodes, 10000, color="#4477AA", dash="dot"     , marker="square-open"            , title="Cont. Steal (local collection)")
            plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc"         , nodes, 10000, color="#004488", dash="dashdot" , marker="circle-open"            , title="Cont. Steal (local collection + greedy join)")
        elif machine == "wisteria-o":
            plot_result(0, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing"    , nodes, 10000, color="#994455", dash="longdash", marker="star-triangle-down-open", title="Child Steal (RtC)")
            plot_result(1, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "child_stealing_ult", nodes, 10000, color="#EE6677", dash="dash"    , marker="star-triangle-up-open"  , title="Child Steal (Full)")
            plot_result(2, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq"             , nodes, 10000, color="#6699CC", dash="solid"   , marker="diamond-open"           , title="Cont. Steal (baseline)")
            plot_result(3, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "waitq_gc"          , nodes, 10000, color="#4477AA", dash="dot"     , marker="square-open"            , title="Cont. Steal (local collection)")
            plot_result(4, fig, "sched_{}_size_n_no_buf".format(comp_pattern), "greedy_gc"         , nodes, 10000, color="#004488", dash="dashdot" , marker="circle-open"            , title="Cont. Steal (local collection + greedy join)")

    if not plot_only_legend:
        # log_labels = ["1", "10", "100", "1K", "10K", "100K", "1M", "10M", "100M", "1G", "10G"]
        log_labels = ["1 ns", "10 ns", "100 ns", "1 us", "10 us", "100 us", "1 ms", "10 ms", "100 ms", "1 s", "10 s"]
        fig.update_xaxes(
            showline=True,
            linecolor='black',
            ticks="outside",
            rangemode="tozero",
            # title_text="N (Problem Size)",
            # title_text="# of tasks",
            # title_text="total work (s)",
            title_text="Ideal Execution Time",
            title_standoff=10,
            type="log",
            tickvals=[(10 ** i) * j for i in range(0, len(log_labels)) for j in range(1, 10)],
            ticktext=[i if j == 1 else "" for i in log_labels for j in range(1, 10)],
            tickangle=0,
            mirror=True,
        )
        fig.update_yaxes(
            showline=True,
            linecolor='black',
            ticks="outside",
            dtick=10,
            # rangemode="tozero",
            # title_text="Scheduling Overhead (%)",
            title_text="Parallel Efficiency (%)",
            title_standoff=10,
            range=[0, 100],
            mirror=True,
        )
        fig.update_layout(
            width=350,
            height=330,
            margin=dict(
                l=0,
                r=5,
                b=0,
                t=0,
            ),
            legend=dict(
                # yanchor="top",
                # y=1,
                yanchor="bottom",
                y=0.22 if eval_pattern == "child_continuation" and comp_pattern == "rrm" else 0,
                xanchor="right",
                x=1,
                itemwidth=itemwidth,
                traceorder="normal" if eval_pattern == "scalability" else "reversed",
            ),
            showlegend=showlegend,
            font=dict(
                family="Linux Biolinum O, sans-serif",
                size=16,
            ),
        )
        fig.write_html("sched_{}_size_n_{}_{}_node_{}.html".format(comp_pattern, eval_pattern, machine, nodes), include_plotlyjs="cdn", config={"toImageButtonOptions" : {"format" : "svg"}})
    else:
        fig.update_xaxes(
            visible=False,
        )
        fig.update_yaxes(
            visible=False,
        )
        fig.update_layout(
            # width=1081,
            # height=60,
            width=1250,
            height=34,
            margin=dict(
                l=0,
                r=0,
                b=0,
                t=0,
            ),
            legend=dict(
                orientation="h",
                yanchor="bottom",
                y=0,
                itemwidth=itemwidth,
                traceorder="normal" if eval_pattern == "scalability" else "reversed",
            ),
            font=dict(
                family="Linux Biolinum O, sans-serif",
                size=16,
            ),
        )
        fig.write_html("sched_{}_size_n_{}_{}_legend.html".format(comp_pattern, eval_pattern, machine), include_plotlyjs="cdn", config={"toImageButtonOptions" : {"format" : "svg"}})

for cp in comp_patterns:
    plot_comp_pattern(cp)
