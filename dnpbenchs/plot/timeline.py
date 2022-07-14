import subprocess
import math
import pandas
import numpy
import plotly.graph_objects as go
import plotly.io as pio
from plotly.subplots import make_subplots

pio.templates.default = "plotly_white"

machine = "ito"

project = "dnpbenchs"

n_bins = 10000

# inset_plot = True
inset_plot = False

x_max = 0
# x_max = 3.2
# x_max = 9.3
# x_max = 10

y2_max = 0
# y2_max = 23000

x_dtick = None
# x_dtick = 0.5

def calc_histogram(df):
    # print("Calculating prefix sum...")
    # df = df.melt(value_vars=["t0", "t1"], var_name = "event", value_name = "time").sort_values("time")
    # df["acc"] = df["event"].replace({"t0": 1, "t1": -1}).cumsum()
    # xs=df["time"]
    # ys=df["acc"]

    # print("Calculating histogram...")
    # xs = numpy.linspace(t_min, t_max, n_bins + 1)
    # ys = numpy.zeros(n_bins + 1)
    # for row in df.itertuples():
    #     t_b = row.t0 - t_min
    #     t_e = row.t1 - t_min
    #     b = math.ceil(t_b / t_delta)
    #     e = math.ceil(t_e / t_delta)
    #     ys[b:e] += 1

    print("Calculating prefix sum...")
    df = df.melt(value_vars=["t0", "t1"], var_name = "event", value_name = "time").sort_values("time")
    df["acc"] = df["event"].replace({"t0": 1, "t1": -1}).cumsum()
    print("Constructing histogram...")
    ts = numpy.linspace(t_min, t_max, n_bins)
    ys = df.iloc[df["time"].searchsorted(ts) - 1]["acc"]
    xs = (ts - t_min) / 1_000_000_000

    return (xs, ys)

isola_path = subprocess.getoutput("isola where {}:test".format(project))
trace_path = "{}/{}/mlog.ignore".format(isola_path, project)

COLUMNS = ["rank0", "t0", "rank1", "t1", "kind", "misc"]

print("Reading CSV...")
df = pandas.read_csv(trace_path, names=COLUMNS)

t_min = df["t0"].min()
t_max = df["t1"].max()
t_delta = (t_max - t_min) / n_bins

np = df["rank0"].max() + 1

fig = make_subplots(specs=[[{"secondary_y": True}]])

(xs, ys) = calc_histogram(df[df["kind"] == "Compute"])
fig.add_trace(go.Scatter(x=xs,
                         y=ys,
                         mode="none",
                         fill="tozeroy",
                         fillcolor="rgba(0, 158, 115, 0.5)",
                         ),
              secondary_y=False)
# fig.add_trace(go.Bar(x=xs,
#                      y=ys,
#                      marker_color="rgba(0, 158, 115, 0.6)",
#                      marker_line_width=0,
#                      marker_pattern_shape="/",
#                      ))
# fig.add_trace(go.Scatter(x=xs,
#                          y=ys,
#                          marker_color="rgba(0, 158, 115, 0.6)",
#                          line_width=1,
#                          stackgroup="one",
#                          ))

(xs, ys) = calc_histogram(df[df["kind"] == "join_outstanding"])
# fig.add_trace(go.Scatter(x=xs,
#                          y=ys,
#                          marker_color="rgba(148, 0, 211, 0.6)",
#                          line_width=1,
#                          stackgroup="one",
#                          ))
fig.add_trace(go.Scatter(x=xs,
                         y=ys,
                         marker_color="rgb(148, 0, 211)",
                         line_color="rgb(148, 0, 211)",
                         line_width=1,
                         ),
              secondary_y=True)

y2_max = max(ys) if y2_max == 0 else y2_max

# fig.add_hline(y=np, line_color="red")

# Ref: https://github.com/VictorBezak/Plotly_Multi-Axes_Gridlines
y1_max = np
y1_dtick = 100
y1_dtick_ratio = y1_max / y1_dtick

tick_value_list = [1, 2, 5]
tick_value_idx = 0
y2_tick_unit = 10**int(math.log10(y2_max) - 1)
y2_dtick = y2_tick_unit
while 1.5 * y2_max > y1_dtick_ratio * y2_dtick:
    tick_value_idx = tick_value_idx + 1;
    y2_dtick = tick_value_list[tick_value_idx % len(tick_value_list)] * y2_tick_unit * (10 ** (tick_value_idx // len(tick_value_list)))

y1_range_max = y1_max
y2_range_max = y1_dtick_ratio * y2_dtick

x_max = max(xs) if x_max == 0 else x_max

if inset_plot:
    zoom_x1 = 0.5
    zoom_x2 = 2.5

    ib = numpy.searchsorted(xs, zoom_x1)
    ie = numpy.searchsorted(xs, zoom_x2, side="right")

    zoom_ymax = max(ys[ib:ie])

    # subplot_x1 = 0.2
    # subplot_x2 = 0.5
    subplot_x1 = zoom_x1 / x_max
    subplot_x2 = zoom_x2 / x_max
    subplot_y1 = 0.15
    subplot_y2 = 0.55

    # background color
    fig.add_trace(go.Scatter(x=[zoom_x1, zoom_x2],
                             y=[zoom_ymax, zoom_ymax],
                             mode="none",
                             fill="tozeroy",
                             fillcolor="rgba(0, 158, 115, 0.3)",
                             xaxis="x3",
                             yaxis="y3"))
    fig.add_trace(go.Scatter(x=xs[ib:ie],
                             y=ys[ib:ie],
                             marker_color="rgb(148, 0, 211)",
                             line_color="rgb(148, 0, 211)",
                             line_width=1,
                             xaxis="x3",
                             yaxis="y3"))

    # fig.add_shape(
    #     type="rect",
    #     x0=zoom_x1,
    #     y0=0,
    #     x1=zoom_x2,
    #     y1=zoom_ymax,
    #     yref="y2",
    #     fillcolor="rgba(148, 0, 211, 0.3)",
    #     line_width=1,
    # )
    fig.add_shape(
        type="line",
        x0=zoom_x1,
        y0=zoom_ymax,
        x1=x_max * subplot_x1,
        y1=y2_range_max * subplot_y1,
        yref="y2",
        line_width=1,
        line_dash="dot",
    )
    fig.add_shape(
        type="line",
        x0=zoom_x2,
        y0=zoom_ymax,
        x1=x_max * subplot_x2,
        y1=y2_range_max * subplot_y1,
        yref="y2",
        line_width=1,
        line_dash="dot",
    )

fig.update_xaxes(
    showline=True,
    linecolor='black',
    ticks="outside",
    rangemode="tozero",
    title_text="Time (s)",
    title_standoff=10,
    tickangle=0,
    range=[0, x_max],
    dtick=x_dtick,
    mirror=True,
)
fig.update_yaxes(
    showline=True,
    linecolor='black',
    ticks="outside",
    title_text="# of Busy Workers",
    title_standoff=10,
    range=[0, y1_range_max],
    dtick=y1_dtick,
    # mirror=True,
    secondary_y=False,
)
fig.update_yaxes(
    showline=True,
    linecolor='black',
    ticks="outside",
    title_text="# of Outstanding Joins",
    title_standoff=10,
    range=[0, y2_range_max],
    dtick=y2_dtick,
    secondary_y=True,
)

x_domain_ratio = fig["layout"]["xaxis"]["domain"][1]

fig.update_layout(
    width=750,
    height=230,
    margin=dict(
        l=0,
        r=0,
        b=0,
        t=0,
    ),
    font=dict(
        # family="Helvetica, sans-serif",
        family="Linux Biolinum O, sans-serif",
        size=16,
    ),
    showlegend=False,
    # bargap=0,
    xaxis3=dict(
        domain=[subplot_x1 * x_domain_ratio, subplot_x2 * x_domain_ratio],
        anchor="y3",
        linecolor="black",
        mirror=True,
        # ticks="outside",
        showticklabels=False,
        range=[zoom_x1, zoom_x2],
    ) if inset_plot else None,
    yaxis3=dict(
        domain=[subplot_y1, subplot_y2],
        anchor="x3",
        linecolor="black",
        mirror=True,
        ticks="outside",
        rangemode="tozero",
        side="right",
        range=[0, zoom_ymax],
    ) if inset_plot else None,
)
fig.write_html("timeline_{}.html".format(machine), include_plotlyjs="cdn", config={"toImageButtonOptions" : {"format" : "svg"}})
