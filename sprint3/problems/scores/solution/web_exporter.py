import prometheus_client as prom
import sys
import json

# Создаём три метрики
good_lines = prom.Counter('webexporter_good_lines', 'Good JSON records')
wrong_lines = prom.Counter('webexporter_wrong_lines', 'Wrong JSON records')

response_time = prom.Histogram('webserver_request_duration', 'Response time', ['code', 'content_type'], 
    buckets=(.001, .002, .005, .010, .020, .050, .100, .200, .500, float("inf")))

def my_main():
    prom.start_http_server(9200)

    for line in sys.stdin:
        try:
            data = json.loads(line)

            if not isinstance(data, dict):
                raise ValueError()

            if data["message"] == "response sent":
                total_time_seconds = data["data"]["response_time"] / 1000
                response_time.labels(
                    code=data["data"]["code"], 
                    content_type=data["data"]["content_type"]).observe(total_time_seconds)

            good_lines.inc()
        except (ValueError, KeyError):
            wrong_lines.inc()

if __name__ == '__main__':
    my_main()