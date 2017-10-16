"""
test_cli_options.py

Tests output generated by Uncrustifys commandline options
(excluding actual source code formatting)

:author:  Daniel Chumak
:license: GPL v2+
"""

from __future__ import print_function
from sys import stderr, argv, version_info, exit as sys_exit
from os import chdir, mkdir, remove, name as os_name
from os.path import dirname, abspath, isdir, isfile
from shutil import rmtree
from subprocess import Popen, PIPE, STDOUT
from io import open
import re

if os_name == 'nt':
    EX_OK = 0
    EX_USAGE = 64
    EX_SOFTWARE = 70
    NULL_DEVICE = 'nul'
else:
    from os import EX_OK, EX_USAGE, EX_SOFTWARE
    NULL_DEVICE = '/dev/null'


def eprint(*args, **kwargs):
    """
        print() wraper that sets file=stderr
    """
    print(*args, file=stderr, **kwargs)


def proc(bin_path, args_arr=(), combine=True):
    """
    simple Popen wrapper to return std out/err utf8 strings


    Parameters
    ----------------------------------------------------------------------------
    :param bin_path: string
        path to the binary that is going to be called

    args_arr : list/tuple
        all needed arguments

    :param combine: bool
        whether or not to merge stderr into stdout


    :return: string, string
    ----------------------------------------------------------------------------
        generated output of both stdout and stderr

    >>> proc("echo", "test")
    'test'
    """
    if not isfile(bin_path):
        eprint("bin is not a file: %s" % bin_path)
        return False

    # call uncrustify, hold output in memory
    stdout_f = PIPE
    stderr_f = PIPE if not combine else STDOUT

    call_arr = [bin_path]
    call_arr.extend(args_arr)
    proc = Popen(call_arr, stdout=stdout_f, stderr=stderr_f)

    out_b, err_b = proc.communicate()
    out_txt = out_b.decode("UTF-8")
    err_txt = err_b.decode("UTF-8") if not combine else None

    return out_txt, err_txt


def get_file_content(fp):
    """
    returns file content as an utf8 string or None if fp is not a file


    Parameters
    ----------------------------------------------------------------------------
    :param fp: string
        path of the file that will be read


    :return: string or None
    ----------------------------------------------------------------------------
    the file content

    """
    out = None

    if isfile(fp):
        with open(fp, encoding="utf-8") as f:
            out = f.read()
    else:
        eprint("is not a file: %s" % fp)

    return out


def check_generated_output(gen_expected_path, gen_result_path, result_manip=None):
    """
    compares the content of two files,

    is intended to compare a file that was generated during a call of Uncrustify
    with a file that has the expected content


    Parameters
    ----------------------------------------------------------------------------
    :param gen_expected_path: string
        path to a file that will be compared with the generated file

    :param gen_result_path: string
        path to the file that will be generated by Uncrustify

    :param result_manip: lambda
        optional lambda function that will be applied (before the comparison)
        on the content of the generated file,
        the lambda function should accept one string parameter


    :return: bool
    ----------------------------------------------------------------------------
    True or False depending on whether both files have the same content

    >>> check_generated_output("/dev/null", "/dev/null")
    True
    """

    gen_exp_txt = get_file_content(gen_expected_path)
    if gen_exp_txt is None:
        return False

    gen_res_txt = get_file_content(gen_result_path)
    if gen_res_txt is None:
        return False

    if result_manip is not None:
        gen_res_txt = result_manip(gen_res_txt)

    if gen_res_txt != gen_exp_txt:

        with open(gen_result_path, 'w', encoding="utf-8") as f:
                f.write(gen_res_txt)

        print("\nProblem with %s" % gen_result_path)
        print("use: 'diff %s %s' to find why" % (gen_result_path,
                                                 gen_expected_path))
        return False

    remove(gen_result_path)

    return True


def check_std_output(expected_path, result_path, result_str, result_manip=None):
    """
    compares output generated by Uncrustify (std out/err) with a the content of
    a file

    Parameters
    ----------------------------------------------------------------------------
    :param expected_path: string
        path of the file that will be compared with the output of Uncrustify

    :param result_path: string
        path to which the Uncrustifys output will be saved in case of a mismatch

    :param result_str: string (utf8)
        the output string generated by Uncrustify

    :param result_manip: lambda
        see result_manip for check_generated_output


    :return: bool
    ----------------------------------------------------------------------------
    True or False depending on whether both files have the same content

    """
    exp_txt = get_file_content(expected_path)
    if exp_txt is None:
        return False

    if result_manip is not None:
        result_str = result_manip(result_str)

    if result_str != exp_txt:
        with open(result_path, 'w', encoding="utf-8") as f:
            f.write(result_str)

        print("\nProblem with %s" % result_path)
        print("use: 'diff %s %s' to find why" % (result_path, expected_path))
        return False
    return True


def check_output(
        uncr_bin, args_arr=(), combine=True,
        out_expected_path=None, out_result_manip=None, out_result_path=None,
        err_expected_path=None, err_result_manip=None, err_result_path=None,
        gen_expected_path=None, gen_result_path=None, gen_result_manip=None):
    """
    compares outputs generated by Uncrustify with files

    Paramerters
    ----------------------------------------------------------------------------
    :param uncr_bin: string
        path to the Uncrustify binary

    :param args_arr: list/tuple
        Uncrustify commandline arguments

    :param combine: bool
        whether or not to merge stderr into stdout, (use ony if
        out_expected_path) is set

    :param out_expected_path: string
        file that will be compared with Uncrustifys stdout output
        (or combined with stderr if combine is True)

    :param out_result_manip: string
        lambda function that will be applied to Uncrustifys stdout output
        (before the comparison with out_expected_path),
        the lambda function should accept one string parameter

    :param out_result_path: string
        path where Uncrustifys stdout output will be saved to in case of a
        mismatch

    :param err_expected_path: string
        path to a file that will be compared with Uncrustifys stderr output

    :param err_result_manip: string
        see out_result_manip (is applied to Uncrustifys stderr instead)

    :param err_result_path: string
        see out_result_path (is applied to Uncrustifys stderr instead)

    :param gen_expected_path: string
        path to a file that will be compared with a file generated by Uncrustify

    :param gen_result_path: string
        path to a file that will be generated by Uncrustify

    :param gen_result_manip:
        see out_result_path (is applied, in memory, to the file content of the
        file generated by Uncrustify insted)


    :return: bool
    ----------------------------------------------------------------------------
    True if all specifed files match up, False otherwise
    """
    # check param sanity
    if not out_expected_path and not err_expected_path and not gen_expected_path:
        eprint("No expected comparison file provided")
        return False

    if combine and (err_result_path or err_expected_path):
        eprint("If combine=True don't set either of err_result_path or "
               "err_expected_path")
        return False

    if bool(gen_expected_path) != bool(gen_result_path):
        eprint("'gen_expected_path' and 'gen_result_path' must be used in "
               "combination")
        return False

    if gen_result_manip and not gen_result_path:
        eprint("Set up 'gen_result_path' if 'gen_result_manip' is used")

    out_res_txt, err_res_txt = proc(uncr_bin, args_arr, combine=combine)

    ret_flag = True

    if out_expected_path and not check_std_output(
            out_expected_path, out_result_path, out_res_txt,
            result_manip=out_result_manip):
        ret_flag = False

    if not combine and err_expected_path and not check_std_output(
            err_expected_path, err_result_path, err_res_txt,
            result_manip=err_result_manip):
        ret_flag = False

    if gen_expected_path and not check_generated_output(
            gen_expected_path, gen_result_path,
            result_manip=gen_result_manip):
        ret_flag = False

    return ret_flag


def clear_dir(path):
    """
    clears a directory by deleting and creating it again


    Parameters
    ----------------------------------------------------------------------------
    :param path:
        path of the directory


    :return: void
    """
    if isdir(path):
        rmtree(path)
    mkdir(path)


def file_find_string(search_string, file_path):
    """
    checks if a strings appears in a file


    Paramerters
    ----------------------------------------------------------------------------
    :param search_string: string
        string that is going to be searched

    :param file_path: string
        file in which the string is going to be searched

    :return: bool
    ----------------------------------------------------------------------------
        True if found, False otherwise
    """
    if isfile(file_path):
        with open(file_path, encoding="utf-8") as f:
            if search_string.lower() in f.read().lower():
                return True
    else:
        eprint("file_path is not a file: %s" % file_path)

    return False


def check_build_type(build_type, cmake_cache_path):
    """
    checks if a cmake build was of a certain single-configuration type


    Parameters:
    ----------------------------------------------------------------------------
    :param build_type: string
        the build type that is going to be expected

    :param cmake_cache_path: string
        the path of the to be checked CMakeCache.txt file


    :return: bool
    ----------------------------------------------------------------------------
    True if the right build type was used, False if not

    """

    check_string = "CMAKE_BUILD_TYPE:STRING=%s" % build_type

    if file_find_string(check_string, cmake_cache_path):
        print("CMAKE_BUILD_TYPE is correct")
        return True

    eprint("CMAKE_BUILD_TYPE must be '%s'" % build_type)
    return False


""" Regex replacement  lambda function, simplistic sed replacement """
reg_replace = lambda pattern, replacement: \
    lambda text: re.sub(pattern, replacement, text)


def main(args):
    # set working dir to script dir
    script_dir = dirname(abspath(__file__))
    chdir(script_dir)

    # find the uncrustify binary (keep Debug dir excluded)
    bin_found = False
    uncr_bin = ''
    uncr_bin_locations = ['../../build/uncrustify',
                          '../../build/Release/uncrustify',
                          '../../build/Release/uncrustify.exe']
    for uncr_bin in uncr_bin_locations:
        if not isfile(uncr_bin):
            eprint("is not a file: %s" % uncr_bin)
        else:
            print("Uncrustify binary found: %s" % uncr_bin)
            bin_found = True
            break
    if not bin_found:
        eprint("No Uncrustify binary found")
        sys_exit(EX_USAGE)

    '''
    Check if the binary was build as Release-type
    
    TODO: find a check for Windows,
          for now rely on the ../../build/Release/ location
    '''
    if os_name != 'nt' \
            and not check_build_type('release', '../../build/CMakeCache.txt'):
        sys_exit(EX_USAGE)

    clear_dir("./Results")

    return_flag = True

    #
    # Test help
    #   -h -? --help --usage
    stdout, stderr = proc(uncr_bin)
    if not check_output(uncr_bin,
                        out_expected_path='./Output/help.txt',
                        out_result_path='./Results/help.txt'):
        return_flag = False

    #
    # Test --show-config
    #
    if not check_output(uncr_bin,
                        args_arr=['--show-config'],
                        out_expected_path='./Output/show_config.txt',
                        out_result_path='./Results/show_config.txt',
                        out_result_manip=reg_replace(r'\# Uncrustify.+', '')):
        return_flag = False

    #
    # Test --update-config
    #
    if not check_output(uncr_bin,
                        args_arr=['-c', './Config/mini_d.cfg',
                                  '--update-config'],
                        out_expected_path='./Output/mini_d_uc.txt',
                        out_result_path='./Results/mini_d_uc.txt',
                        out_result_manip=reg_replace(r'\# Uncrustify.+', '')):
        return_flag = False

    if not check_output(uncr_bin,
                        args_arr=['-c', './Config/mini_nd.cfg',
                                  '--update-config'],
                        out_expected_path='./Output/mini_nd_uc.txt',
                        out_result_path='./Results/mini_nd_uc.txt',
                        out_result_manip=reg_replace(r'\# Uncrustify.+', '')):
        return_flag = False

    #
    # Test --update-config-with-doc
    #
    if not check_output(uncr_bin,
                        args_arr=['-c', './Config/mini_d.cfg',
                                  '--update-config-with-doc'],
                        out_expected_path='./Output/mini_d_ucwd.txt',
                        out_result_path='./Results/mini_d_ucwd.txt',
                        out_result_manip=reg_replace(r'\# Uncrustify.+', '')):
        return_flag = False

    if not check_output(uncr_bin,
                        args_arr=['-c', './Config/mini_nd.cfg',
                                  '--update-config-with-doc'],
                        out_expected_path='./Output/mini_nd_ucwd.txt',
                        out_result_path='./Results/mini_nd_ucwd.txt',
                        out_result_manip=reg_replace(r'\# Uncrustify.+', '')):
        return_flag = False

    #
    # Test -p
    #
    if not check_output(uncr_bin,
                        args_arr=['-c', './Config/mini_nd.cfg',
                                  '-f', './Input/28.cpp',
                                  '-p', "./Results/p.txt"],
                        gen_expected_path='./Output/p.txt',
                        gen_result_path='./Results/p.txt',
                        gen_result_manip=reg_replace(r'\# Uncrustify.+', '')):
        return_flag = False

    # Debug Options:
    #   -L
    # look at src/log_levels.h
    Ls_A = ['9', '21', '25', '28', '31', '36', '66', '92']
    for L in Ls_A:
        if not check_output(uncr_bin,
                            args_arr=[
                                        '-c', NULL_DEVICE,
                                        '-f', './Input/%s.cpp' % L,
                                        '-o', NULL_DEVICE,
                                        '-L', L],
                            combine=False,
                            err_expected_path='./Output/%s.txt' % L,
                            err_result_path='./Results/%s.txt' % L,
                            err_result_manip=reg_replace(r'[0-9]', '')):
            return_flag = False

    error_tests = ["I-842", "unmatched_close_pp"]
    for test in error_tests:
        if not check_output(uncr_bin,
                            args_arr=[
                                '-q',
                                '-c', './Config/%s.cfg' % test,
                                '-f', './Input/%s.cpp' % test,
                                '-o', NULL_DEVICE],
                            combine=False,
                            err_expected_path='./Output/%s.txt' % test,
                            err_result_path='./Results/%s.txt' % test):
            return_flag = False

    if return_flag:
        print("all tests are OK")
        sys_exit(EX_OK)
    else:
        print("some problem(s) are still present")
        sys_exit(EX_SOFTWARE)


if __name__ == "__main__":
    main(argv[1:])
