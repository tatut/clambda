# C Lambda

This is a custom AWS Lambda runtime for C.

## Quickstart

* Build image and bootstrap: `make build-image build`
* Build a handler: `./build-handler.sh example`

To test locally:, build and run the runtime-image:
* Build runtime image: `make runtime-image`
* Run a handler: `docker run -p 8080:8080 -t clambda/runtime example.analyze_line`
* call via cURL: `curl http://localhost:8080/2015-03-31/functions/function/invocations -d '{"id": "viiva", "points":[{"x": 10, "y": 10}, {"y":20, "x": 100}, {"x": 200, "y": 222}]}'`

(see `example.c` for other examples)


## Deploying to actual AWS environment

Detailed instructions on [AWS documentation site](https://docs.aws.amazon.com/lambda/latest/dg/runtimes-walkthrough.html).

Once you have built the bootstrap and your handler (`bootstrap` and some `handler.o` file exist).

Steps:
* Make sure they have execute bits (`chmod 755`)
* Zip it `zip mylambda.zip bootstrap handler.o` (use `-X` on Mac to discard extended attributes)
* Deploy using AWS cli: ```
aws lambda create-function --function-name mylambda \
                           --zip-file fileb://mylambda.zip \
                           --handler handler.handler_fn_name \
                           --runtime provided.al2023 \
                           --role arn:aws:iam::$MY_ACCOUNT_ID:role/lambda-role```

* Invoke using AWS cli: ```
aws lambda invoke --function-name mylambda \
                  --payload '{"text":"Hello"}' response.txt \
                  --cli-binary-format raw-in-base64-out```
