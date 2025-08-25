namespace Tweaker.Core.Errors;

public record Error(string Message, string? Code = null, Exception? InnerException = null);

public abstract record Result<T>;
public sealed record Success<T>(T Value) : Result<T>;
public sealed record Failure<T>(Error Value) : Result<T>;

public static class ResultImpl
{
    public static Result<T> ToFailure<T>(this string message) => new Failure<T>(new Error(message));
    public static Result<T> ToFailure<T>(this string message, string code) => new Failure<T>(new Error(message, code));
    public static Result<T> ToFailure<T>(this string message, string code, Exception exception)
        => new Failure<T>(new Error(message, code, exception));

    public static Result<T> ToSuccess<T>(this T value) => new Success<T>(value);

    public static Result<TOut> Map<TIn, TOut>(
        this Result<TIn> result, Func<TIn, TOut> mapper)
    {
        return result switch
        {
            Success<TIn>(var value) => new Success<TOut>(mapper(value)),
            Failure<TIn>(var error) => new Failure<TOut>(error),
            _ => throw new InvalidOperationException()
        };
    }

    public static Result<TOut> Bind<TIn, TOut>(
        this Result<TIn> result, Func<TIn, Result<TOut>> binder)
    {
        return result switch
        {
            Success<TIn>(var value) => binder(value),
            Failure<TIn>(var error) => new Failure<TOut>(error),
            _ => throw new InvalidOperationException()
        };
    }

    public static TResult Match<T, TResult>(
        this Result<T> result, Func<T, TResult> onSuccess, Func<Error, TResult> onError)
    {
        return result switch
        {
            Success<T>(var value) => onSuccess(value),
            Failure<T>(var error) => onError(error),
            _ => throw new InvalidOperationException(),
        };
    }

    public static void Match<T>(
        this Result<T> result, Action<T> onSuccess, Action<Error> onError)
    {
        switch (result)
        {
            case Success<T>(var value):
                onSuccess(value);
                break;
            case Failure<T>(var error):
                onError(error);
                break;
            default:
                throw new InvalidOperationException();
        }
    }
}